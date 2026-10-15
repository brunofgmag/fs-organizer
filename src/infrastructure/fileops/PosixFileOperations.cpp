#include "infrastructure/fileops/PosixFileOperations.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <QtCore/QFile>

#include "infrastructure/fileops/FileDescriptor.h"
#include "support/PathText.h"

namespace
{
    constexpr std::size_t kBytesCopiedAtATime = 1024 * 1024;

    struct WalkedFile
    {
        std::filesystem::path path{};
        std::uintmax_t bytes = 0;
    };

    struct Walk
    {
        std::vector<WalkedFile> files{};
        std::uintmax_t totalBytes = 0;
    };

    enum class Copied
    {
        Done,
        Cancelled,
        Failed,
    };

    struct CopyState
    {
        const std::function<bool(const CopyProgress&)>* onProgress = nullptr;
        std::uintmax_t copiedBefore = 0;
        std::uintmax_t totalBytes = 0;
    };

    std::optional<Walk> FilesUnder(const std::filesystem::path& root)
    {
        std::error_code error;
        std::filesystem::recursive_directory_iterator entry(
            root, std::filesystem::directory_options::skip_permission_denied, error);
        if (error)
        {
            return std::nullopt;
        }

        Walk walk;
        const std::filesystem::recursive_directory_iterator end;

        while (entry != end)
        {
            const bool isFile = entry->is_regular_file(error);
            if (error)
            {
                return std::nullopt;
            }

            if (isFile)
            {
                const std::uintmax_t bytes = entry->file_size(error);
                if (error)
                {
                    return std::nullopt;
                }

                walk.files.push_back({.path = entry->path(), .bytes = bytes});
                walk.totalBytes += bytes;
            }

            entry.increment(error);
            if (error)
            {
                return std::nullopt;
            }
        }

        return walk;
    }

    bool WroteAll(const int descriptor, const char* bytes, const ssize_t count)
    {
        ssize_t written = 0;

        while (written < count)
        {
            const ssize_t now = ::write(descriptor, bytes + written, static_cast<std::size_t>(count - written));
            if (now < 0 && errno == EINTR)
            {
                continue;
            }

            if (now <= 0)
            {
                return false;
            }

            written += now;
        }

        return true;
    }

    bool KeepsTheMarksOf(const std::filesystem::path& from, const std::filesystem::path& to)
    {
        std::error_code error;
        const std::filesystem::file_status status = std::filesystem::status(from, error);
        if (error)
        {
            return false;
        }

        std::filesystem::permissions(to, status.permissions(), error);
        if (error)
        {
            return false;
        }

        const std::filesystem::file_time_type written = std::filesystem::last_write_time(from, error);
        if (error)
        {
            return false;
        }

        std::filesystem::last_write_time(to, written, error);

        return !error;
    }

    bool Reported(const CopyState& state, const std::uintmax_t transferred)
    {
        if (state.onProgress == nullptr || !*state.onProgress)
        {
            return true;
        }

        return (*state.onProgress)(
            CopyProgress{.copiedBytes = state.copiedBefore + transferred, .totalBytes = state.totalBytes});
    }

    Copied CopyOneFile(const std::filesystem::path& from, const std::filesystem::path& to, CopyState& state)
    {
        const FileDescriptor source(::open(from.c_str(), O_RDONLY | O_CLOEXEC));
        if (!source.IsOpen())
        {
            return Copied::Failed;
        }

        const FileDescriptor target(::open(to.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600));
        if (!target.IsOpen())
        {
            return Copied::Failed;
        }

        std::string block(kBytesCopiedAtATime, '\0');
        std::uintmax_t transferred = 0;
        Copied outcome = Copied::Done;

        while (outcome == Copied::Done)
        {
            const ssize_t read = ::read(source.Value(), block.data(), block.size());
            if (read < 0 && errno == EINTR)
            {
                continue;
            }

            if (read == 0)
            {
                break;
            }

            if (read < 0 || !WroteAll(target.Value(), block.data(), read))
            {
                outcome = Copied::Failed;
                break;
            }

            transferred += static_cast<std::uintmax_t>(read);

            if (!Reported(state, transferred))
            {
                outcome = Copied::Cancelled;
            }
        }

        if (outcome == Copied::Done && !KeepsTheMarksOf(from, to))
        {
            outcome = Copied::Failed;
        }

        if (outcome != Copied::Done)
        {
            std::error_code discarded;
            std::filesystem::remove(to, discarded);
        }

        return outcome;
    }

    bool IsAPhysicalFolder(const std::filesystem::path& path)
    {
        std::error_code error;

        return std::filesystem::is_directory(std::filesystem::symlink_status(path, error));
    }

    bool SomethingIsAt(const std::filesystem::path& path)
    {
        std::error_code error;
        const std::filesystem::file_status status = std::filesystem::symlink_status(path, error);

        return !error && std::filesystem::exists(status);
    }
}

CopyOutcome PosixFileOperations::CopyTree(const std::filesystem::path& source,
                                          const std::filesystem::path& destination,
                                          const std::function<bool(const CopyProgress&)>& onProgress)
{
    const std::optional<Walk> walk = FilesUnder(source);
    if (!walk.has_value())
    {
        return CopyOutcome::Failed;
    }

    std::error_code error;
    std::filesystem::create_directories(destination, error);
    if (error)
    {
        return CopyOutcome::Failed;
    }

    CopyState state{.onProgress = &onProgress, .copiedBefore = 0, .totalBytes = walk->totalBytes};

    for (const WalkedFile& file : walk->files)
    {
        const std::filesystem::path landing = destination / file.path.lexically_relative(source);

        std::filesystem::create_directories(landing.parent_path(), error);
        if (error)
        {
            return CopyOutcome::Failed;
        }

        const Copied copied = CopyOneFile(file.path, landing, state);
        if (copied != Copied::Done)
        {
            return copied == Copied::Cancelled ? CopyOutcome::Cancelled : CopyOutcome::Failed;
        }

        state.copiedBefore += file.bytes;
    }

    return CopyOutcome::Completed;
}

bool PosixFileOperations::CreateFolder(const std::filesystem::path& path)
{
    if (SomethingIsAt(path))
    {
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(path, error);

    return !error;
}

bool PosixFileOperations::WriteHiddenFile(const std::filesystem::path& path)
{
    const FileDescriptor file(::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644));

    return file.IsOpen();
}

bool PosixFileOperations::Move(const std::filesystem::path& source, const std::filesystem::path& destination)
{
    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error)
    {
        return false;
    }

    if (::renameat2(AT_FDCWD, source.c_str(), AT_FDCWD, destination.c_str(), RENAME_NOREPLACE) == 0)
    {
        return true;
    }

    if (errno != EINVAL || SomethingIsAt(destination))
    {
        return false;
    }

    std::filesystem::rename(source, destination, error);

    return !error;
}

bool PosixFileOperations::RemoveEmptyFolder(const std::filesystem::path& path)
{
    if (!IsAPhysicalFolder(path))
    {
        return false;
    }

    std::error_code error;

    return std::filesystem::remove(path, error) && !error;
}

bool PosixFileOperations::Recycle(const std::filesystem::path& path)
{
    if (!SomethingIsAt(path))
    {
        return false;
    }

    return QFile::moveToTrash(AsText(path));
}

bool PosixFileOperations::RemoveTree(const std::filesystem::path& path)
{
    if (!SomethingIsAt(path))
    {
        return false;
    }

    std::error_code error;
    std::filesystem::remove_all(path, error);

    return !error;
}
