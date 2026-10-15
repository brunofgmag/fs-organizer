#include "infrastructure/archive/FolderArchiveReader.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>

#include "domain/support/PathUtils.h"
#include "infrastructure/archive/EmptyStagingFolder.h"
#include "infrastructure/archive/FolderEntryPaths.h"
#include "infrastructure/fileops/ExtendedPaths.h"

namespace
{
    using Clock = std::chrono::steady_clock;

    struct Source
    {
        std::filesystem::path root{};
        std::string name{};
    };

    constexpr std::chrono::milliseconds kTick{50};
    constexpr std::chrono::milliseconds kHeartbeat{250};
    constexpr std::chrono::milliseconds kWorkerGrace{2000};

    enum class Ending
    {
        Finished,
        Cancelled,
        Stalled,
    };

    struct Pace
    {
        std::uintmax_t copied = 0;
        Clock::time_point moved{};
        Clock::time_point reported{};
    };

    struct Piece
    {
        std::filesystem::path from{};
        std::filesystem::path to{};
        std::uintmax_t bytes = 0;
        bool isFolder = false;
    };

    [[nodiscard]] DWORD AttributesOf(const std::filesystem::path& path)
    {
        return GetFileAttributesW(path.c_str());
    }

    [[nodiscard]] bool IsAReparsePoint(const DWORD attributes)
    {
        return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    }

    [[nodiscard]] bool IsADirectory(const DWORD attributes)
    {
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    [[nodiscard]] bool IsAPlainFile(const std::filesystem::path& path)
    {
        const DWORD attributes = AttributesOf(path);

        return attributes != INVALID_FILE_ATTRIBUTES && !IsADirectory(attributes) && !IsAReparsePoint(attributes);
    }

    [[nodiscard]] std::optional<std::string> ConvertedToUtf8(const std::wstring& text, const DWORD flags)
    {
        const int length = static_cast<int>(text.size());
        const int size = WideCharToMultiByte(CP_UTF8, flags, text.data(), length, nullptr, 0, nullptr, nullptr);
        if (size <= 0)
        {
            return std::nullopt;
        }

        std::string utf8(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, flags, text.data(), length, utf8.data(), size, nullptr, nullptr);

        return utf8;
    }

    [[nodiscard]] std::optional<std::string> ExactNameOf(const std::filesystem::path& name)
    {
        return ConvertedToUtf8(name.wstring(), WC_ERR_INVALID_CHARS);
    }

    [[nodiscard]] std::string ReadableNameOf(const std::filesystem::path& name)
    {
        return ConvertedToUtf8(name.wstring(), 0).value_or(std::string{});
    }

    [[nodiscard]] std::optional<Source> SourceOf(const std::filesystem::path& folder)
    {
        std::filesystem::path root = WithExtendedPrefix(folder);
        if (!root.has_filename())
        {
            root = root.parent_path();
        }

        std::optional<std::string> name = ExactNameOf(root.filename());
        if (!name.has_value())
        {
            return std::nullopt;
        }

        return Source{.root = std::move(root), .name = std::move(*name)};
    }

    [[nodiscard]] bool IsAPhysicalFolder(const std::filesystem::path& root)
    {
        const DWORD attributes = AttributesOf(root);

        return attributes != INVALID_FILE_ATTRIBUTES && IsADirectory(attributes) && !IsAReparsePoint(attributes);
    }

    [[nodiscard]] ArchiveFailure ScanInto(const std::filesystem::path& folder,
                                          const std::string& prefix,
                                          Clock::time_point deadline,
                                          std::vector<ArchiveEntry>& entries);

    [[nodiscard]] std::optional<std::vector<std::filesystem::path>> ChildrenOf(const std::filesystem::path& folder)
    {
        std::error_code error;
        std::vector<std::filesystem::path> children;
        std::filesystem::directory_iterator child(folder, error);
        const std::filesystem::directory_iterator end;

        while (!error && child != end)
        {
            children.push_back(child->path());
            child.increment(error);
        }

        if (error)
        {
            return std::nullopt;
        }

        return children;
    }

    [[nodiscard]] ArchiveFailure ScanChild(const std::filesystem::path& path,
                                           const std::string& prefix,
                                           const Clock::time_point deadline,
                                           std::vector<ArchiveEntry>& entries)
    {
        const std::optional<std::string> name = ExactNameOf(path.filename());
        if (!name.has_value())
        {
            entries.push_back(ArchiveEntry{
                .path = prefix + "/" + ReadableNameOf(path.filename()), .bytes = 0, .kind = ArchiveEntryKind::Other});

            return ArchiveFailure::None;
        }

        const std::string listed = prefix + "/" + *name;
        const DWORD attributes = AttributesOf(path);

        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            return ArchiveFailure::CouldNotOpen;
        }
        if (IsAReparsePoint(attributes))
        {
            entries.push_back(ArchiveEntry{.path = listed, .bytes = 0, .kind = ArchiveEntryKind::Other});

            return ArchiveFailure::None;
        }
        if (IsADirectory(attributes))
        {
            entries.push_back(ArchiveEntry{.path = listed, .bytes = 0, .kind = ArchiveEntryKind::Folder});

            return ScanInto(path, listed, deadline, entries);
        }

        std::error_code error;
        const std::uintmax_t bytes = std::filesystem::file_size(path, error);
        if (error)
        {
            return ArchiveFailure::CouldNotOpen;
        }

        entries.push_back(ArchiveEntry{.path = listed, .bytes = bytes, .kind = ArchiveEntryKind::File});

        return ArchiveFailure::None;
    }

    [[nodiscard]] ArchiveFailure ScanInto(const std::filesystem::path& folder,
                                          const std::string& prefix,
                                          const Clock::time_point deadline,
                                          std::vector<ArchiveEntry>& entries)
    {
        const std::optional<std::vector<std::filesystem::path>> children = ChildrenOf(folder);
        if (!children.has_value())
        {
            return ArchiveFailure::CouldNotOpen;
        }

        for (const std::filesystem::path& child : *children)
        {
            if (Clock::now() >= deadline)
            {
                return ArchiveFailure::TookTooLong;
            }

            if (const ArchiveFailure failure = ScanChild(child, prefix, deadline, entries);
                failure != ArchiveFailure::None)
            {
                return failure;
            }
        }

        return ArchiveFailure::None;
    }

    class CopyJob
    {
    public:
        CopyJob(std::vector<Piece> pieces, const std::uintmax_t totalBytes, const bool listened)
            : pieces_(std::move(pieces)), totalBytes_(totalBytes), listened_(listened)
        {
        }

        void Run()
        {
            const ArchiveFailure outcome = Perform();

            const std::scoped_lock lock(guard_);
            outcome_ = outcome;
            finished_ = true;
            changed_.notify_all();
        }

        [[nodiscard]] Ending Watch(const std::function<bool(const CopyProgress&)>& onProgress,
                                   const std::chrono::milliseconds idleLimit)
        {
            const Clock::time_point start = Clock::now();
            Pace pace{.copied = copied_.load(), .moved = start, .reported = start};
            std::unique_lock lock(guard_);

            while (!finished_)
            {
                changed_.wait_for(lock, kTick,
                                  [this]
                                  {
                                      return finished_ || asking_;
                                  });
                if (finished_)
                {
                    break;
                }

                NoteMovement(pace);

                if ((asking_ || Clock::now() - pace.reported >= kHeartbeat) && !Reported(lock, onProgress, pace))
                {
                    return Ending::Cancelled;
                }

                if (Clock::now() - pace.moved >= idleLimit)
                {
                    return Ending::Stalled;
                }
            }

            return Ending::Finished;
        }

        void Stop()
        {
            {
                const std::scoped_lock lock(guard_);
                stopped_ = true;
            }
            changed_.notify_all();
            std::atomic_ref<BOOL>(cancelFlag_).store(TRUE);
        }

        [[nodiscard]] bool WaitUntilFinished(const std::chrono::milliseconds within)
        {
            std::unique_lock lock(guard_);

            return changed_.wait_for(lock, within,
                                     [this]
                                     {
                                         return finished_;
                                     });
        }

        [[nodiscard]] ArchiveFailure Outcome()
        {
            const std::scoped_lock lock(guard_);

            return outcome_;
        }

        [[nodiscard]] DWORD Chunk(const std::uintmax_t transferred)
        {
            if (Ask(copiedBefore_ + transferred))
            {
                return PROGRESS_CONTINUE;
            }

            declined_ = true;

            return PROGRESS_CANCEL;
        }

    private:
        void NoteMovement(Pace& pace) const
        {
            const std::uintmax_t copied = copied_.load();
            if (copied != pace.copied)
            {
                pace.copied = copied;
                pace.moved = Clock::now();
            }
        }

        [[nodiscard]] bool Reported(std::unique_lock<std::mutex>& lock,
                                    const std::function<bool(const CopyProgress&)>& onProgress,
                                    Pace& pace)
        {
            const bool answering = asking_;
            const Clock::time_point began = Clock::now();

            lock.unlock();
            const bool keepGoing =
                !onProgress || onProgress(CopyProgress{.copiedBytes = pace.copied, .totalBytes = totalBytes_});
            lock.lock();

            if (answering)
            {
                keepGoing_ = keepGoing;
                asking_ = false;
                changed_.notify_all();
            }
            pace.reported = Clock::now();
            pace.moved += pace.reported - began;

            return keepGoing;
        }

        [[nodiscard]] bool Ask(const std::uintmax_t copied)
        {
            copied_.store(copied);
            if (!listened_)
            {
                return !stopped_;
            }

            std::unique_lock lock(guard_);
            asking_ = true;
            changed_.notify_all();
            changed_.wait(lock,
                          [this]
                          {
                              return !asking_ || stopped_;
                          });

            return !stopped_ && keepGoing_;
        }

        [[nodiscard]] ArchiveFailure Perform()
        {
            for (const Piece& piece : pieces_)
            {
                if (stopped_)
                {
                    return ArchiveFailure::Cancelled;
                }

                const ArchiveFailure done = piece.isFolder ? MakeFolder(piece) : CopyOne(piece);
                if (done != ArchiveFailure::None)
                {
                    return done;
                }

                copiedBefore_ += piece.isFolder ? 0 : piece.bytes;
                if (!piece.isFolder && !Ask(copiedBefore_))
                {
                    return ArchiveFailure::Cancelled;
                }
            }

            return ArchiveFailure::None;
        }

        [[nodiscard]] static ArchiveFailure MakeFolder(const Piece& piece)
        {
            std::error_code error;
            std::filesystem::create_directories(piece.to, error);

            return error ? ArchiveFailure::CouldNotExtract : ArchiveFailure::None;
        }

        [[nodiscard]] ArchiveFailure CopyOne(const Piece& piece)
        {
            std::error_code error;
            std::filesystem::create_directories(piece.to.parent_path(), error);
            if (error || !IsAPlainFile(piece.from))
            {
                return ArchiveFailure::CouldNotExtract;
            }

            if (CopyFileExW(piece.from.c_str(), piece.to.c_str(), ReportProgress, this, &cancelFlag_,
                            COPY_FILE_FAIL_IF_EXISTS)
                == FALSE)
            {
                return declined_ ? ArchiveFailure::Cancelled : ArchiveFailure::CouldNotExtract;
            }

            return ArchiveFailure::None;
        }

        static DWORD CALLBACK ReportProgress(const LARGE_INTEGER,
                                             const LARGE_INTEGER transferred,
                                             const LARGE_INTEGER,
                                             const LARGE_INTEGER,
                                             const DWORD,
                                             const DWORD,
                                             const HANDLE,
                                             const HANDLE,
                                             const LPVOID data)
        {
            return static_cast<CopyJob*>(data)->Chunk(static_cast<std::uintmax_t>(transferred.QuadPart));
        }

        std::vector<Piece> pieces_;
        std::uintmax_t totalBytes_;
        bool listened_;
        std::mutex guard_{};
        std::condition_variable changed_{};
        std::atomic<std::uintmax_t> copied_{0};
        std::atomic<bool> stopped_{false};
        bool asking_ = false;
        bool keepGoing_ = false;
        bool finished_ = false;
        ArchiveFailure outcome_ = ArchiveFailure::None;
        std::uintmax_t copiedBefore_ = 0;
        bool declined_ = false;
        BOOL cancelFlag_ = FALSE;
    };

    [[nodiscard]] std::vector<Piece> PiecesOf(const std::vector<ArchiveEntry>& listed,
                                              const std::vector<std::filesystem::path>& relative,
                                              const std::filesystem::path& from,
                                              const std::filesystem::path& to)
    {
        std::vector<Piece> pieces;
        pieces.reserve(listed.size());

        for (std::size_t at = 0; at < listed.size(); ++at)
        {
            pieces.push_back(Piece{.from = from / relative[at],
                                   .to = to / relative[at],
                                   .bytes = listed[at].bytes,
                                   .isFolder = listed[at].kind == ArchiveEntryKind::Folder});
        }

        return pieces;
    }

    [[nodiscard]] HANDLE DuplicateOf(std::thread& worker)
    {
        HANDLE copy = nullptr;
        const BOOL duplicated = DuplicateHandle(GetCurrentProcess(), worker.native_handle(), GetCurrentProcess(), &copy,
                                                0, FALSE, DUPLICATE_SAME_ACCESS);

        return duplicated != FALSE ? copy : nullptr;
    }

    void KeepCancellingItsIo(const std::shared_ptr<CopyJob>& job, const HANDLE worker)
    {
        const Clock::time_point until = Clock::now() + kWorkerGrace;
        bool finished = false;
        while (!finished && Clock::now() < until)
        {
            CancelSynchronousIo(worker);
            finished = job->WaitUntilFinished(kTick);
        }

        CloseHandle(worker);
    }

    void CancelOffTheCallingThread(std::thread& worker, const std::shared_ptr<CopyJob>& job)
    {
        const HANDLE duplicate = DuplicateOf(worker);
        if (duplicate == nullptr)
        {
            return;
        }

        std::thread(
            [job, duplicate]
            {
                KeepCancellingItsIo(job, duplicate);
            })
            .detach();
    }

    [[nodiscard]] ArchiveFailure GivenUp(std::thread& worker, const std::shared_ptr<CopyJob>& job, const Ending ending)
    {
        job->Stop();
        CancelOffTheCallingThread(worker, job);

        if (job->WaitUntilFinished(kWorkerGrace))
        {
            worker.join();
        }
        else
        {
            worker.detach();
        }

        return ending == Ending::Stalled ? ArchiveFailure::TookTooLong : ArchiveFailure::Cancelled;
    }
}

FolderArchiveReader::FolderArchiveReader(const std::chrono::milliseconds timeLimit,
                                         const std::chrono::milliseconds idleLimit)
    : timeLimit_(timeLimit), idleLimit_(idleLimit)
{
}

ArchiveListing FolderArchiveReader::EntriesOf(const std::filesystem::path& folder) const
{
    const Clock::time_point deadline = Clock::now() + timeLimit_;
    const std::optional<Source> source = SourceOf(folder);

    if (!source.has_value() || !IsAPhysicalFolder(source->root))
    {
        return {.failure = ArchiveFailure::CouldNotOpen, .entries = {}};
    }

    ArchiveListing listing{
        .failure = ArchiveFailure::None,
        .entries = {ArchiveEntry{.path = source->name, .bytes = 0, .kind = ArchiveEntryKind::Folder}}};

    listing.failure = ScanInto(source->root, source->name, deadline, listing.entries);
    if (listing.failure != ArchiveFailure::None)
    {
        listing.entries.clear();
    }

    return listing;
}

ArchiveText FolderArchiveReader::ContentsOf(const std::filesystem::path& folder,
                                            const std::string& entry,
                                            const std::size_t most) const
{
    const std::optional<Source> source = SourceOf(folder);
    const std::optional<std::filesystem::path> below =
        source.has_value() ? BelowTheFolder(entry, source->name) : std::nullopt;

    if (!below.has_value() || below->empty() || !IsAPlainFile(source->root / *below))
    {
        return {.failure = ArchiveFailure::CouldNotOpen, .text = {}};
    }

    const std::filesystem::path file = source->root / *below;
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(file, error);
    std::ifstream stream(file, std::ios::binary);

    if (error || !stream.is_open())
    {
        return {.failure = ArchiveFailure::CouldNotOpen, .text = {}};
    }

    std::string text(static_cast<std::size_t>(std::min<std::uintmax_t>(size, most)), '\0');
    stream.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(stream.gcount()));

    return {.failure = ArchiveFailure::None, .text = std::move(text)};
}

ArchiveFailure FolderArchiveReader::ExtractAll(const std::filesystem::path& folder,
                                               const std::vector<ArchiveEntry>& listed,
                                               const std::filesystem::path& into,
                                               const std::function<bool(const CopyProgress&)>& onProgress) const
{
    const std::optional<Source> source = SourceOf(folder);
    if (!source.has_value() || !IsAnEmptyStagingFolder(into))
    {
        return ArchiveFailure::CouldNotExtract;
    }

    const std::optional<std::vector<std::filesystem::path>> relative = RelativePathsOf(listed, source->name);
    if (!relative.has_value())
    {
        return ArchiveFailure::CouldNotExtract;
    }

    const std::filesystem::path landing = WithExtendedPrefix(into) / PathFromUtf8(source->name);
    const auto job = std::make_shared<CopyJob>(PiecesOf(listed, *relative, source->root, landing),
                                               BytesOfTheFiles(listed), static_cast<bool>(onProgress));

    std::thread worker(
        [job]
        {
            job->Run();
        });
    const Ending ending = job->Watch(onProgress, idleLimit_);
    if (ending != Ending::Finished)
    {
        return GivenUp(worker, job, ending);
    }

    worker.join();

    return job->Outcome();
}
