#include "infrastructure/archive/PosixFolderArchiveReader.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>

#include <QtCore/QByteArrayView>
#include <QtCore/QString>

#include "domain/support/PathUtils.h"
#include "infrastructure/archive/EmptyStagingFolder.h"
#include "infrastructure/archive/FolderEntryPaths.h"
#include "infrastructure/fileops/FileDescriptor.h"

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
    constexpr std::size_t kBytesCopiedAtATime = 1024 * 1024;

    enum class Ending
    {
        Finished,
        Cancelled,
        Stalled,
    };

    enum class Copied
    {
        Done,
        Cancelled,
        Failed,
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

    [[nodiscard]] std::optional<std::string> ExactNameOf(const std::filesystem::path& name)
    {
        std::string text = AsUtf8(name);
        if (text.empty() || !QByteArrayView(text).isValidUtf8())
        {
            return std::nullopt;
        }

        return text;
    }

    [[nodiscard]] std::string ReadableNameOf(const std::filesystem::path& name)
    {
        return QString::fromUtf8(AsUtf8(name)).toUtf8().toStdString();
    }

    [[nodiscard]] std::optional<Source> SourceOf(const std::filesystem::path& folder)
    {
        std::error_code error;
        std::filesystem::path root = std::filesystem::absolute(folder, error).lexically_normal();
        if (error)
        {
            return std::nullopt;
        }

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

    [[nodiscard]] std::filesystem::file_type TypeWithoutFollowing(const std::filesystem::path& path)
    {
        std::error_code error;

        return std::filesystem::symlink_status(path, error).type();
    }

    [[nodiscard]] bool IsAPhysicalFolder(const std::filesystem::path& root)
    {
        return TypeWithoutFollowing(root) == std::filesystem::file_type::directory;
    }

    [[nodiscard]] FileDescriptor OpenedPlainFile(const std::filesystem::path& file)
    {
        FileDescriptor descriptor(::open(file.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));

        struct stat status
        {
        };

        if (descriptor.IsOpen() && (::fstat(descriptor.Value(), &status) != 0 || !S_ISREG(status.st_mode)))
        {
            return FileDescriptor(-1);
        }

        return descriptor;
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

        std::ranges::sort(children);

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

        switch (TypeWithoutFollowing(path))
        {
        case std::filesystem::file_type::none:
        case std::filesystem::file_type::not_found: return ArchiveFailure::CouldNotOpen;
        case std::filesystem::file_type::directory:
            entries.push_back(ArchiveEntry{.path = listed, .bytes = 0, .kind = ArchiveEntryKind::Folder});

            return ScanInto(path, listed, deadline, entries);
        case std::filesystem::file_type::regular: break;
        default:
            entries.push_back(ArchiveEntry{.path = listed, .bytes = 0, .kind = ArchiveEntryKind::Other});

            return ArchiveFailure::None;
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
            if (error)
            {
                return ArchiveFailure::CouldNotExtract;
            }

            const FileDescriptor source = OpenedPlainFile(piece.from);
            if (!source.IsOpen())
            {
                return ArchiveFailure::CouldNotExtract;
            }

            struct stat status
            {
            };

            if (::fstat(source.Value(), &status) != 0)
            {
                return ArchiveFailure::CouldNotExtract;
            }

            const FileDescriptor target(
                ::open(piece.to.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, status.st_mode & 0777));
            if (!target.IsOpen())
            {
                return ArchiveFailure::CouldNotExtract;
            }

            Copied copied = Copy(source.Value(), target.Value());
            if (copied == Copied::Done)
            {
                const timespec times[2] = {{.tv_sec = 0, .tv_nsec = UTIME_OMIT}, status.st_mtim};
                static_cast<void>(::futimens(target.Value(), times));
            }
            else
            {
                std::error_code discarded;
                std::filesystem::remove(piece.to, discarded);
            }

            switch (copied)
            {
            case Copied::Done: return ArchiveFailure::None;
            case Copied::Cancelled: return ArchiveFailure::Cancelled;
            case Copied::Failed: break;
            }

            return ArchiveFailure::CouldNotExtract;
        }

        [[nodiscard]] Copied Copy(const int source, const int target)
        {
            std::string block(kBytesCopiedAtATime, '\0');
            std::uintmax_t transferred = 0;

            while (true)
            {
                const ssize_t read = ::read(source, block.data(), block.size());
                if (read < 0 && errno == EINTR)
                {
                    continue;
                }

                if (read == 0)
                {
                    return Copied::Done;
                }

                if (stopped_)
                {
                    return Copied::Cancelled;
                }

                if (read < 0 || !WroteAll(target, block.data(), static_cast<std::size_t>(read)))
                {
                    return Copied::Failed;
                }

                transferred += static_cast<std::uintmax_t>(read);

                if (!Ask(copiedBefore_ + transferred))
                {
                    return Copied::Cancelled;
                }
            }
        }

        [[nodiscard]] static bool WroteAll(const int descriptor, const char* bytes, const std::size_t count)
        {
            std::size_t written = 0;

            while (written < count)
            {
                const ssize_t now = ::write(descriptor, bytes + written, count - written);
                if (now < 0 && errno == EINTR)
                {
                    continue;
                }

                if (now <= 0)
                {
                    return false;
                }

                written += static_cast<std::size_t>(now);
            }

            return true;
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

    [[nodiscard]] ArchiveFailure GivenUp(std::thread& worker, const std::shared_ptr<CopyJob>& job, const Ending ending)
    {
        job->Stop();

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

PosixFolderArchiveReader::PosixFolderArchiveReader(const std::chrono::milliseconds timeLimit,
                                                   const std::chrono::milliseconds idleLimit)
    : timeLimit_(timeLimit), idleLimit_(idleLimit)
{
}

ArchiveListing PosixFolderArchiveReader::EntriesOf(const std::filesystem::path& folder) const
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

ArchiveText PosixFolderArchiveReader::ContentsOf(const std::filesystem::path& folder,
                                                 const std::string& entry,
                                                 const std::size_t most) const
{
    const std::optional<Source> source = SourceOf(folder);
    const std::optional<std::filesystem::path> below =
        source.has_value() ? BelowTheFolder(entry, source->name) : std::nullopt;

    if (!below.has_value() || below->empty())
    {
        return {.failure = ArchiveFailure::CouldNotOpen, .text = {}};
    }

    const FileDescriptor file = OpenedPlainFile(source->root / *below);
    if (!file.IsOpen())
    {
        return {.failure = ArchiveFailure::CouldNotOpen, .text = {}};
    }

    std::string text;
    std::string block(std::min(most, kBytesCopiedAtATime), '\0');

    while (text.size() < most)
    {
        const ssize_t read = ::read(file.Value(), block.data(), std::min(block.size(), most - text.size()));
        if (read < 0 && errno == EINTR)
        {
            continue;
        }

        if (read < 0)
        {
            return {.failure = ArchiveFailure::CouldNotOpen, .text = {}};
        }

        if (read == 0)
        {
            break;
        }

        text.append(block, 0, static_cast<std::size_t>(read));
    }

    return {.failure = ArchiveFailure::None, .text = std::move(text)};
}

ArchiveFailure PosixFolderArchiveReader::ExtractAll(const std::filesystem::path& folder,
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

    const std::filesystem::path landing = std::filesystem::absolute(into) / PathFromUtf8(source->name);
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
