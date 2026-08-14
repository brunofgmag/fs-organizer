#ifndef FS_ORGANIZER_TESTS_SUPPORT_STD_FILESYSTEM_PROBE_H
#define FS_ORGANIZER_TESTS_SUPPORT_STD_FILESYSTEM_PROBE_H

#include <cstddef>
#include <fstream>
#include <iterator>
#include <system_error>

#include <QtCore/QByteArrayView>
#include <QtCore/QCryptographicHash>

#include "domain/ports/FilesystemProbe.h"
#include "support/FileClock.h"

#include "infrastructure/fileops/ExtendedPaths.h"

[[nodiscard]] inline std::filesystem::path AsFarAsTheProductionProbeReaches(const std::filesystem::path& path)
{
    return WithExtendedPrefix(path);
}

[[nodiscard]] inline std::filesystem::path WithoutTheReachPrefix(const std::filesystem::path& path)
{
    return WithoutExtendedPrefix(path);
}

class StdFilesystemProbe final : public FilesystemProbe
{
public:
    [[nodiscard]] static bool IsALinkWithoutFollowing(const std::filesystem::file_status& status)
    {
        if (std::filesystem::is_symlink(status))
        {
            return true;
        }

        return status.type() == std::filesystem::file_type::junction;
    }

    [[nodiscard]] bool EntryExistsWithoutFollowingLinks(const std::filesystem::path& path) const override
    {
        std::error_code error;

        return std::filesystem::symlink_status(AsFarAsTheProductionProbeReaches(path), error).type()
            != std::filesystem::file_type::not_found;
    }

    [[nodiscard]] bool TargetDirectoryExists(const std::filesystem::path& path) const override
    {
        std::error_code error;

        return std::filesystem::is_directory(AsFarAsTheProductionProbeReaches(path), error);
    }

    [[nodiscard]] bool IsReparsePoint(const std::filesystem::path& path) const override
    {
        std::error_code error;

        return IsALinkWithoutFollowing(std::filesystem::symlink_status(AsFarAsTheProductionProbeReaches(path), error));
    }

    [[nodiscard]] bool PhysicalDirectoryExists(const std::filesystem::path& path) const override
    {
        std::error_code error;
        const std::filesystem::file_status status =
            std::filesystem::symlink_status(AsFarAsTheProductionProbeReaches(path), error);

        return status.type() == std::filesystem::file_type::directory && !IsALinkWithoutFollowing(status);
    }

    [[nodiscard]] std::vector<std::filesystem::path> ChildDirectories(const std::filesystem::path& path) const override
    {
        std::vector<std::filesystem::path> children;

        std::error_code error;
        for (const std::filesystem::directory_entry& entry :
             std::filesystem::directory_iterator(AsFarAsTheProductionProbeReaches(path),
                                                 std::filesystem::directory_options::skip_permission_denied, error))
        {
            const std::filesystem::file_status status = std::filesystem::symlink_status(entry.path(), error);

            if (status.type() == std::filesystem::file_type::directory || IsALinkWithoutFollowing(status))
            {
                children.push_back(path / entry.path().filename());
            }
        }

        return children;
    }

    [[nodiscard]] std::vector<std::filesystem::path> ChildFiles(const std::filesystem::path& path) const override
    {
        std::vector<std::filesystem::path> children;

        std::error_code error;
        for (const std::filesystem::directory_entry& entry :
             std::filesystem::directory_iterator(AsFarAsTheProductionProbeReaches(path),
                                                 std::filesystem::directory_options::skip_permission_denied, error))
        {
            const std::filesystem::file_status status = std::filesystem::symlink_status(entry.path(), error);

            if (status.type() == std::filesystem::file_type::regular)
            {
                children.push_back(path / entry.path().filename());
            }
        }

        return children;
    }

    [[nodiscard]] bool VolumeIsAvailable(const std::filesystem::path&) const override
    {
        return true;
    }

    [[nodiscard]] WriteAccess ProbeWritable(const std::filesystem::path& path) const override
    {
        return TargetDirectoryExists(path) ? WriteAccess::ItAccepts : WriteAccess::TheFolderIsNotThere;
    }

    [[nodiscard]] std::optional<std::uintmax_t> FreeSpaceOn(const std::filesystem::path& path) const override
    {
        std::error_code error;
        const std::filesystem::space_info space = std::filesystem::space(AsFarAsTheProductionProbeReaches(path), error);

        return error ? std::nullopt : std::optional(space.available);
    }

    [[nodiscard]] std::optional<RecycleBinRoom> RecycleBinOn(const std::filesystem::path&) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point>
    LastWriteTime(const std::filesystem::path& path) const override
    {
        std::error_code error;
        const std::filesystem::file_time_type written =
            std::filesystem::last_write_time(AsFarAsTheProductionProbeReaches(path), error);

        return error ? std::nullopt : std::optional(SystemTimeOf(written));
    }

    [[nodiscard]] std::optional<std::string> ContentsOf(const std::filesystem::path& path) const override
    {
        std::ifstream file(AsFarAsTheProductionProbeReaches(path), std::ios::binary);
        if (!file.is_open())
        {
            return std::nullopt;
        }

        return std::string(std::istreambuf_iterator(file), std::istreambuf_iterator<char>());
    }

    [[nodiscard]] std::optional<std::string> FirstBytesOf(const std::filesystem::path& path,
                                                          const std::size_t most) const override
    {
        std::ifstream file(AsFarAsTheProductionProbeReaches(path), std::ios::binary);
        if (!file.is_open())
        {
            return std::nullopt;
        }

        std::string bytes(most, '\0');
        file.read(bytes.data(), static_cast<std::streamsize>(most));
        bytes.resize(static_cast<std::size_t>(file.gcount()));

        return bytes;
    }

    [[nodiscard]] std::optional<std::string> HashOf(const std::filesystem::path& path) const override
    {
        std::ifstream file(AsFarAsTheProductionProbeReaches(path), std::ios::binary);
        if (!file.is_open())
        {
            return std::nullopt;
        }

        constexpr std::size_t kBytesReadAtATime = 1024 * 1024;

        QCryptographicHash digest(QCryptographicHash::Sha256);

        std::string block(kBytesReadAtATime, '\0');
        while (file.read(block.data(), static_cast<std::streamsize>(block.size())) || file.gcount() > 0)
        {
            digest.addData(QByteArrayView(block.data(), static_cast<qsizetype>(file.gcount())));
        }

        return digest.result().toHex().toStdString();
    }

    [[nodiscard]] std::optional<TreeFingerprint> FingerprintTree(const std::filesystem::path& root) const override
    {
        const std::filesystem::path reachableRoot = AsFarAsTheProductionProbeReaches(root);

        std::error_code error;
        std::filesystem::recursive_directory_iterator entry(
            reachableRoot, std::filesystem::directory_options::skip_permission_denied, error);
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

            if (const std::size_t here = WithoutTheReachPrefix(entry->path()).native().size();
                here > walked.longestEntry)
            {
                walked.longestEntry = here;
            }

            if (isFile)
            {
                const std::uintmax_t size = entry->file_size(error);
                if (error)
                {
                    return std::nullopt;
                }

                walked.files.push_back(
                    FileFingerprint{.relativePath = entry->path().lexically_relative(reachableRoot), .size = size});
            }

            entry.increment(error);
            if (error)
            {
                return std::nullopt;
            }
        }

        return walked;
    }
};

#endif // FS_ORGANIZER_TESTS_SUPPORT_STD_FILESYSTEM_PROBE_H
