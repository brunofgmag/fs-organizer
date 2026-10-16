#include "infrastructure/archive/LibarchiveArchiveReader.h"

#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <locale.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <system_error>

#include <QtCore/QString>

#include "domain/support/CaseFolding.h"
#include "domain/support/PathSegment.h"
#include "domain/support/PathUtils.h"
#include "infrastructure/archive/EmptyStagingFolder.h"
#include "infrastructure/fileops/FileDescriptor.h"

namespace
{
    using Clock = std::chrono::steady_clock;

    constexpr std::size_t kBytesPerBlock = 64 * 1024;
    constexpr std::chrono::milliseconds kHeartbeat{250};
    constexpr std::array<const char*, 2> kUtf8Locales{"C.UTF-8", "en_US.UTF-8"};
    constexpr auto kUnreadableName = "?";

    struct ReadCloser
    {
        void operator()(archive* handle) const
        {
            archive_read_free(handle);
        }
    };

    using ReadHandle = std::unique_ptr<archive, ReadCloser>;

    enum class Next
    {
        Entry,
        End,
        Encrypted,
        Damaged,
    };

    struct Described
    {
        std::string path{};
        std::uintmax_t bytes = 0;
        ArchiveEntryKind kind = ArchiveEntryKind::File;
        bool dataIsEncrypted = false;
    };

    class Utf8Names
    {
    public:
        Utf8Names()
        {
            for (const char* name : kUtf8Locales)
            {
                utf8_ = newlocale(LC_CTYPE_MASK, name, nullptr);
                if (utf8_ != nullptr)
                {
                    break;
                }
            }

            if (utf8_ != nullptr)
            {
                before_ = uselocale(utf8_);
            }
        }

        Utf8Names(const Utf8Names&) = delete;

        Utf8Names& operator=(const Utf8Names&) = delete;

        ~Utf8Names()
        {
            if (utf8_ != nullptr)
            {
                uselocale(before_);
                freelocale(utf8_);
            }
        }

    private:
        locale_t utf8_ = nullptr;
        locale_t before_ = nullptr;
    };

    [[nodiscard]] ReadHandle Opened(const std::filesystem::path& source)
    {
        ReadHandle handle(archive_read_new());
        if (!handle)
        {
            return nullptr;
        }

        static_cast<void>(archive_read_support_format_zip(handle.get()));
        static_cast<void>(archive_read_support_format_7zip(handle.get()));
        static_cast<void>(archive_read_support_format_rar(handle.get()));
        static_cast<void>(archive_read_support_format_rar5(handle.get()));

        if (archive_read_open_filename(handle.get(), source.c_str(), kBytesPerBlock) != ARCHIVE_OK)
        {
            return nullptr;
        }

        return handle;
    }

    [[nodiscard]] Next NextEntry(archive& handle, archive_entry*& entry)
    {
        switch (archive_read_next_header(&handle, &entry))
        {
        case ARCHIVE_OK:
        case ARCHIVE_WARN: return Next::Entry;
        case ARCHIVE_EOF: return Next::End;
        default: return archive_read_has_encrypted_entries(&handle) > 0 ? Next::Encrypted : Next::Damaged;
        }
    }

    [[nodiscard]] std::string NormalisedPath(std::string path)
    {
        std::ranges::replace(path, '\\', '/');
        while (path.starts_with("./"))
        {
            path.erase(0, 2);
        }
        while (path.ends_with('/'))
        {
            path.pop_back();
        }

        return path == "." ? std::string{} : path;
    }

    [[nodiscard]] std::string ReadableName(archive_entry& entry)
    {
        const char* raw = archive_entry_pathname(&entry);

        return raw == nullptr ? kUnreadableName : QString::fromUtf8(raw).toUtf8().toStdString();
    }

    [[nodiscard]] bool IsALink(archive_entry& entry)
    {
        return archive_entry_hardlink(&entry) != nullptr || archive_entry_symlink(&entry) != nullptr;
    }

    [[nodiscard]] ArchiveEntryKind KindOf(archive_entry& entry)
    {
        if (IsALink(entry))
        {
            return ArchiveEntryKind::Other;
        }

        switch (archive_entry_filetype(&entry))
        {
        case AE_IFREG: return ArchiveEntryKind::File;
        case AE_IFDIR: return ArchiveEntryKind::Folder;
        default: return ArchiveEntryKind::Other;
        }
    }

    [[nodiscard]] Described DescribedBy(archive_entry& entry)
    {
        const char* exact = archive_entry_pathname_utf8(&entry);
        const ArchiveEntryKind kind = exact == nullptr ? ArchiveEntryKind::Other : KindOf(entry);
        const bool isAFile = kind == ArchiveEntryKind::File;

        return Described{
            .path = NormalisedPath(exact == nullptr ? ReadableName(entry) : std::string(exact)),
            .bytes = isAFile && archive_entry_size_is_set(&entry) != 0
                ? static_cast<std::uintmax_t>(archive_entry_size(&entry))
                : std::uintmax_t{0},
            .kind = kind,
            .dataIsEncrypted = isAFile && archive_entry_is_data_encrypted(&entry) != 0,
        };
    }

    [[nodiscard]] ArchiveFailure FailureOf(const Next ending, const ArchiveFailure otherwise)
    {
        return ending == Next::Encrypted ? ArchiveFailure::NeedsAPassword : otherwise;
    }

    [[nodiscard]] bool WroteAll(const int descriptor, const char* bytes, const std::size_t count, const off_t at)
    {
        std::size_t written = 0;

        while (written < count)
        {
            const ssize_t now =
                ::pwrite(descriptor, bytes + written, count - written, at + static_cast<off_t>(written));
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

    class Extraction
    {
    public:
        Extraction(archive& handle,
                   std::filesystem::path into,
                   const std::uintmax_t totalBytes,
                   const std::function<bool(const CopyProgress&)>& onProgress,
                   const std::chrono::milliseconds idleLimit)
            : handle_(handle),
              into_(std::move(into)),
              onProgress_(onProgress),
              idleLimit_(idleLimit),
              progress_{.copiedBytes = 0, .totalBytes = totalBytes}
        {
        }

        [[nodiscard]] ArchiveFailure Run()
        {
            if (!Report())
            {
                return ArchiveFailure::Cancelled;
            }

            archive_entry* entry = nullptr;
            for (Next next = NextEntry(handle_, entry); next != Next::End; next = NextEntry(handle_, entry))
            {
                if (next != Next::Entry)
                {
                    return FailureOf(next, ArchiveFailure::CouldNotExtract);
                }

                if (const ArchiveFailure failure = Place(*entry); failure != ArchiveFailure::None)
                {
                    return failure;
                }
            }

            return Report() ? ArchiveFailure::None : ArchiveFailure::Cancelled;
        }

    private:
        [[nodiscard]] std::optional<std::filesystem::path> RelativePathOf(const std::string& entry,
                                                                          const bool isAFolder)
        {
            std::filesystem::path relative;
            std::string place;
            std::size_t from = 0;

            while (true)
            {
                const std::size_t slash = entry.find('/', from);
                const bool isLast = slash == std::string::npos;
                const std::optional<PathSegment> segment =
                    PathSegment::From(entry.substr(from, isLast ? slash : slash - from));

                if (!segment.has_value())
                {
                    return std::nullopt;
                }

                place += "/" + LoweredForComparison(segment->Text());
                const bool namesAFolder = !isLast || isAFolder;
                const std::string& spelled = namesAFolder
                    ? foldersSpelledAs_.try_emplace(place, segment->Text()).first->second
                    : segment->Text();

                relative /= PathFromUtf8(spelled);

                if (isLast)
                {
                    return relative;
                }

                from = slash + 1;
            }
        }

        [[nodiscard]] bool Report()
        {
            reported_ = Clock::now();

            return !onProgress_ || onProgress_(progress_);
        }

        [[nodiscard]] ArchiveFailure Place(archive_entry& entry)
        {
            const Described described = DescribedBy(entry);
            if (described.kind == ArchiveEntryKind::Other)
            {
                return ArchiveFailure::CouldNotExtract;
            }

            if (described.path.empty())
            {
                return ArchiveFailure::None;
            }

            const std::optional<std::filesystem::path> relative =
                RelativePathOf(described.path, described.kind == ArchiveEntryKind::Folder);
            if (!relative.has_value())
            {
                return ArchiveFailure::CouldNotExtract;
            }

            const std::filesystem::path landing = into_ / *relative;
            std::error_code error;

            if (described.kind == ArchiveEntryKind::Folder)
            {
                std::filesystem::create_directories(landing, error);

                return error ? ArchiveFailure::CouldNotExtract : ArchiveFailure::None;
            }

            if (described.dataIsEncrypted && described.bytes > 0)
            {
                return ArchiveFailure::NeedsAPassword;
            }

            std::filesystem::create_directories(landing.parent_path(), error);
            if (error)
            {
                return ArchiveFailure::CouldNotExtract;
            }

            const ArchiveFailure written = WriteFile(landing);

            return written != ArchiveFailure::None || Report() ? written : ArchiveFailure::Cancelled;
        }

        [[nodiscard]] ArchiveFailure WriteFile(const std::filesystem::path& landing)
        {
            const FileDescriptor file(
                ::open(landing.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644));
            if (!file.IsOpen())
            {
                return errno == EEXIST ? ArchiveFailure::None : ArchiveFailure::CouldNotExtract;
            }

            moved_ = Clock::now();

            while (true)
            {
                const void* block = nullptr;
                std::size_t size = 0;
                la_int64_t offset = 0;
                const int result = archive_read_data_block(&handle_, &block, &size, &offset);

                if (result == ARCHIVE_EOF)
                {
                    return ArchiveFailure::None;
                }

                if (result != ARCHIVE_OK || offset < 0
                    || !WroteAll(file.Value(), static_cast<const char*>(block), size, static_cast<off_t>(offset)))
                {
                    return ArchiveFailure::CouldNotExtract;
                }

                if (const ArchiveFailure stopped = AfterABlock(size); stopped != ArchiveFailure::None)
                {
                    return stopped;
                }
            }
        }

        [[nodiscard]] ArchiveFailure AfterABlock(const std::size_t size)
        {
            if (Clock::now() - moved_ >= idleLimit_)
            {
                return ArchiveFailure::TookTooLong;
            }

            progress_.copiedBytes = std::min(progress_.copiedBytes + size, progress_.totalBytes);
            moved_ = Clock::now();

            if (moved_ - reported_ >= kHeartbeat && !Report())
            {
                return ArchiveFailure::Cancelled;
            }

            return ArchiveFailure::None;
        }

        archive& handle_;
        std::filesystem::path into_;
        const std::function<bool(const CopyProgress&)>& onProgress_;
        std::chrono::milliseconds idleLimit_;
        CopyProgress progress_;
        std::map<std::string, std::string> foldersSpelledAs_{};
        Clock::time_point reported_{};
        Clock::time_point moved_{};
    };

    [[nodiscard]] std::uintmax_t BytesOfTheFiles(const std::vector<ArchiveEntry>& listed)
    {
        std::uintmax_t total = 0;

        for (const ArchiveEntry& entry : listed)
        {
            total += entry.kind == ArchiveEntryKind::File ? entry.bytes : 0;
        }

        return total;
    }

    [[nodiscard]] ArchiveText ReadFrom(archive& handle, const std::size_t most)
    {
        std::string text;
        std::string block(std::min(most, kBytesPerBlock), '\0');

        while (text.size() < most)
        {
            const la_ssize_t read =
                archive_read_data(&handle, block.data(), std::min(block.size(), most - text.size()));
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
}

LibarchiveArchiveReader::LibarchiveArchiveReader(const std::chrono::milliseconds timeLimit,
                                                 const std::chrono::milliseconds idleLimit)
    : timeLimit_(timeLimit), idleLimit_(idleLimit)
{
}

ArchiveListing LibarchiveArchiveReader::EntriesOf(const std::filesystem::path& source) const
{
    const Utf8Names names;
    const ReadHandle handle = Opened(source);
    if (!handle)
    {
        return {.failure = ArchiveFailure::CouldNotOpen, .entries = {}};
    }

    const Clock::time_point deadline = Clock::now() + timeLimit_;
    ArchiveListing listing;
    archive_entry* entry = nullptr;
    Next next = NextEntry(*handle, entry);

    for (; next == Next::Entry; next = NextEntry(*handle, entry))
    {
        if (Clock::now() >= deadline)
        {
            return {.failure = ArchiveFailure::TookTooLong, .entries = {}};
        }

        Described described = DescribedBy(*entry);
        if (!described.path.empty())
        {
            listing.entries.push_back(
                ArchiveEntry{.path = std::move(described.path), .bytes = described.bytes, .kind = described.kind});
        }
    }

    if (next != Next::End)
    {
        return {.failure = FailureOf(next, ArchiveFailure::CouldNotOpen), .entries = {}};
    }

    return listing;
}

ArchiveText LibarchiveArchiveReader::ContentsOf(const std::filesystem::path& source,
                                                const std::string& entry,
                                                const std::size_t most) const
{
    const Utf8Names names;
    const ReadHandle handle = Opened(source);
    if (!handle)
    {
        return {.failure = ArchiveFailure::CouldNotOpen, .text = {}};
    }

    const Clock::time_point deadline = Clock::now() + timeLimit_;
    archive_entry* current = nullptr;
    Next next = NextEntry(*handle, current);

    for (; next == Next::Entry; next = NextEntry(*handle, current))
    {
        if (Clock::now() >= deadline)
        {
            return {.failure = ArchiveFailure::TookTooLong, .text = {}};
        }

        const Described described = DescribedBy(*current);
        if (described.path != entry)
        {
            continue;
        }

        if (described.kind != ArchiveEntryKind::File)
        {
            return {.failure = ArchiveFailure::CouldNotOpen, .text = {}};
        }

        if (described.dataIsEncrypted && described.bytes > 0)
        {
            return {.failure = ArchiveFailure::NeedsAPassword, .text = {}};
        }

        return ReadFrom(*handle, most);
    }

    return {.failure = FailureOf(next, ArchiveFailure::CouldNotOpen), .text = {}};
}

ArchiveFailure LibarchiveArchiveReader::ExtractAll(const std::filesystem::path& source,
                                                   const std::vector<ArchiveEntry>& listed,
                                                   const std::filesystem::path& into,
                                                   const std::function<bool(const CopyProgress&)>& onProgress) const
{
    if (!IsAnEmptyStagingFolder(into))
    {
        return ArchiveFailure::CouldNotExtract;
    }

    const Utf8Names names;
    const ReadHandle handle = Opened(source);
    if (!handle)
    {
        return ArchiveFailure::CouldNotExtract;
    }

    Extraction extraction(*handle, std::filesystem::absolute(into), BytesOfTheFiles(listed), onProgress, idleLimit_);

    return extraction.Run();
}
