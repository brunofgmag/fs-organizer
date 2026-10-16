#include "infrastructure/fileops/PosixFilesystemProbe.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include <QtCore/QByteArrayView>
#include <QtCore/QCryptographicHash>

#include "domain/support/PathUtils.h"
#include "infrastructure/fileops/MountPoints.h"
#include "support/FileClock.h"
#include "support/FileReading.h"

namespace
{
    constexpr std::size_t kBytesReadAtATime = 1024 * 1024;
    constexpr auto kWriteProbeName = ".fsorg-write-probe";
    constexpr unsigned int kMountIdMask = 0x00001000U;

    bool IsAFolderOrALinkThatMayBeOne(const std::filesystem::directory_entry& entry)
    {
        std::error_code error;
        if (!entry.is_symlink(error))
        {
            return entry.is_directory(error);
        }

        const bool resolvesToAFolder = entry.is_directory(error);

        return resolvesToAFolder || !std::filesystem::exists(entry.path(), error);
    }

    template<typename Wanted>
    std::vector<std::filesystem::path> ChildrenWhoseKindPasses(const std::filesystem::path& path, const Wanted& wanted)
    {
        std::error_code error;
        std::vector<std::filesystem::path> children;

        for (std::filesystem::directory_iterator entries(path, error), end; !error && entries != end;
             entries.increment(error))
        {
            if (wanted(IsAFolderOrALinkThatMayBeOne(*entries)))
            {
                children.push_back(entries->path());
            }
        }

        if (error)
        {
            return {};
        }

        std::ranges::sort(children);

        return children;
    }

    std::optional<std::string> TextOfTheFile(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream.is_open())
        {
            return std::nullopt;
        }

        return BytesRemainingIn(stream);
    }

    WriteAccess WhatTheProbeRanInto(const int error)
    {
        switch (error)
        {
        case EACCES:
        case EPERM: return WriteAccess::PermissionIsDenied;
        case EROFS: return WriteAccess::TheVolumeIsReadOnly;
        default: return WriteAccess::ItRefusedForAnotherReason;
        }
    }
}

bool PosixFilesystemProbe::EntryExistsWithoutFollowingLinks(const std::filesystem::path& path) const
{
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::symlink_status(path, error);

    return !error && std::filesystem::exists(status);
}

bool PosixFilesystemProbe::TargetDirectoryExists(const std::filesystem::path& path) const
{
    std::error_code error;

    return std::filesystem::is_directory(path, error);
}

bool PosixFilesystemProbe::IsReparsePoint(const std::filesystem::path& path) const
{
    std::error_code error;

    return std::filesystem::is_symlink(std::filesystem::symlink_status(path, error));
}

bool PosixFilesystemProbe::PhysicalDirectoryExists(const std::filesystem::path& path) const
{
    std::error_code error;

    return std::filesystem::is_directory(std::filesystem::symlink_status(path, error));
}

std::vector<std::filesystem::path> PosixFilesystemProbe::ChildDirectories(const std::filesystem::path& path) const
{
    return ChildrenWhoseKindPasses(path,
                                   [](const bool isAFolder)
                                   {
                                       return isAFolder;
                                   });
}

std::vector<std::filesystem::path> PosixFilesystemProbe::ChildFiles(const std::filesystem::path& path) const
{
    return ChildrenWhoseKindPasses(path,
                                   [](const bool isAFolder)
                                   {
                                       return !isAFolder;
                                   });
}

PosixFilesystemProbe::PosixFilesystemProbe(std::filesystem::path mountInfo, std::filesystem::path fstab)
    : mountInfo_(std::move(mountInfo)), fstab_(std::move(fstab))
{
}

bool PosixFilesystemProbe::VolumeIsAvailable(const std::filesystem::path& path) const
{
    if (!path.is_absolute())
    {
        return false;
    }

    const std::optional<std::string> mounted = TextOfTheFile(mountInfo_);
    if (!mounted.has_value())
    {
        return true;
    }

    return !TheMountIsMissing(path, MountedPointsIn(*mounted),
                              ConfiguredPointsIn(TextOfTheFile(fstab_).value_or(std::string{})));
}

std::optional<std::string> PosixFilesystemProbe::VolumeIdentityOf(const std::filesystem::path& path) const
{
    if (!VolumeIsAvailable(path))
    {
        return std::nullopt;
    }

    std::filesystem::path place = path.lexically_normal();

    struct statx described
    {
    };

    while (
        ::statx(AT_FDCWD, place.c_str(), AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT, STATX_TYPE | kMountIdMask, &described)
        != 0)
    {
        if ((errno != ENOENT && errno != ENOTDIR) || !place.has_relative_path())
        {
            return std::nullopt;
        }

        place = place.parent_path();
    }

    const std::string device = std::to_string(described.stx_dev_major) + ":" + std::to_string(described.stx_dev_minor);
    const bool knowsTheMount = (described.stx_mask & kMountIdMask) != 0;

    return knowsTheMount ? device + ":" + std::to_string(described.stx_mnt_id) : device;
}

WriteAccess PosixFilesystemProbe::ProbeWritable(const std::filesystem::path& path) const
{
    if (!TargetDirectoryExists(path))
    {
        return WriteAccess::TheFolderIsNotThere;
    }

    const std::filesystem::path probe = path / kWriteProbeName;
    const int descriptor = ::open(probe.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (descriptor < 0)
    {
        return WhatTheProbeRanInto(errno);
    }

    ::close(descriptor);

    std::error_code discarded;
    std::filesystem::remove(probe, discarded);

    return WriteAccess::ItAccepts;
}

bool PosixFilesystemProbe::SomethingIsHoldingItOpen(const std::filesystem::path&) const
{
    return false;
}

std::optional<std::uintmax_t> PosixFilesystemProbe::FreeSpaceOn(const std::filesystem::path& path) const
{
    std::error_code error;
    const std::filesystem::space_info space = std::filesystem::space(path, error);
    if (error)
    {
        return std::nullopt;
    }

    return space.available;
}

std::optional<RecycleBinRoom> PosixFilesystemProbe::RecycleBinOn(const std::filesystem::path&) const
{
    return std::nullopt;
}

std::optional<std::string> PosixFilesystemProbe::ContentsOf(const std::filesystem::path& path) const
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return std::nullopt;
    }

    return BytesRemainingIn(file);
}

std::optional<std::string> PosixFilesystemProbe::FirstBytesOf(const std::filesystem::path& path,
                                                              const std::size_t most) const
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return std::nullopt;
    }

    std::string bytes(most, '\0');
    file.read(bytes.data(), static_cast<std::streamsize>(most));
    if (file.bad())
    {
        return std::nullopt;
    }

    bytes.resize(static_cast<std::size_t>(file.gcount()));

    return bytes;
}

std::optional<std::string> PosixFilesystemProbe::HashOf(const std::filesystem::path& path) const
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return std::nullopt;
    }

    QCryptographicHash digest(QCryptographicHash::Sha256);

    std::string block(kBytesReadAtATime, '\0');
    while (file.read(block.data(), static_cast<std::streamsize>(block.size())) || file.gcount() > 0)
    {
        digest.addData(QByteArrayView(block.data(), static_cast<qsizetype>(file.gcount())));
    }

    return file.bad() ? std::nullopt : std::optional(digest.result().toHex().toStdString());
}

std::vector<std::optional<std::string>>
PosixFilesystemProbe::HashesOf(const std::filesystem::path& root, const std::vector<std::filesystem::path>& below) const
{
    std::vector<std::optional<std::string>> digests(below.size());
    std::atomic<std::size_t> next{0};

    const auto hashWhatIsLeft = [this, &root, &below, &digests, &next]
    {
        for (std::size_t place = next++; place < below.size(); place = next++)
        {
            digests[place] = HashOf(PathUnder(root, below[place]));
        }
    };

    const std::size_t helpers = std::min<std::size_t>(std::max(1U, std::thread::hardware_concurrency()), below.size());

    std::vector<std::thread> workers;
    for (std::size_t worker = 1; worker < helpers; ++worker)
    {
        workers.emplace_back(hashWhatIsLeft);
    }

    hashWhatIsLeft();

    for (std::thread& worker : workers)
    {
        worker.join();
    }

    return digests;
}

std::optional<TreeFingerprint> PosixFilesystemProbe::FingerprintTree(const std::filesystem::path& root) const
{
    std::error_code error;
    std::filesystem::recursive_directory_iterator entry(
        root, std::filesystem::directory_options::skip_permission_denied, error);
    if (error)
    {
        return std::nullopt;
    }

    TreeFingerprint walked{.longestEntry = root.native().size()};
    const std::filesystem::recursive_directory_iterator end;

    while (entry != end)
    {
        const bool isFile = entry->is_regular_file(error);
        if (error)
        {
            return std::nullopt;
        }

        walked.longestEntry = std::max(walked.longestEntry, entry->path().native().size());

        if (isFile)
        {
            const std::uintmax_t size = entry->file_size(error);
            if (error)
            {
                return std::nullopt;
            }

            const std::filesystem::file_time_type written = entry->last_write_time(error);
            if (error)
            {
                return std::nullopt;
            }

            walked.files.push_back(FileFingerprint{.relativePath = entry->path().lexically_relative(root),
                                                   .size = size,
                                                   .lastWriteTime = SystemTimeOf(written)});
        }

        entry.increment(error);
        if (error)
        {
            return std::nullopt;
        }
    }

    return walked;
}

std::optional<std::chrono::system_clock::time_point>
PosixFilesystemProbe::LastWriteTime(const std::filesystem::path& path) const
{
    std::error_code error;
    const std::filesystem::file_time_type written = std::filesystem::last_write_time(path, error);
    if (error)
    {
        return std::nullopt;
    }

    return SystemTimeOf(written);
}
