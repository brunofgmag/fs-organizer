#ifndef FS_ORGANIZER_TESTS_SUPPORT_STD_FILESYSTEM_PROBE_H
#define FS_ORGANIZER_TESTS_SUPPORT_STD_FILESYSTEM_PROBE_H

#include <system_error>

#include "domain/ports/FilesystemProbe.h"
#include "support/FileClock.h"

class StdFilesystemProbe final : public FilesystemProbe
{
public:
    [[nodiscard]] static bool IsALinkWithoutFollowing(const std::filesystem::file_status& status)
    {
        if (std::filesystem::is_symlink(status))
        {
            return true;
        }

#ifdef _WIN32
        return status.type() == std::filesystem::file_type::junction;
#else
        return false;
#endif
    }

    [[nodiscard]] bool EntryExistsWithoutFollowingLinks(const std::filesystem::path& path) const override
    {
        std::error_code error;

        return std::filesystem::symlink_status(path, error).type() != std::filesystem::file_type::not_found;
    }

    [[nodiscard]] bool TargetDirectoryExists(const std::filesystem::path& path) const override
    {
        std::error_code error;

        return std::filesystem::is_directory(path, error);
    }

    [[nodiscard]] bool IsReparsePoint(const std::filesystem::path& path) const override
    {
        std::error_code error;

        return IsALinkWithoutFollowing(std::filesystem::symlink_status(path, error));
    }

    [[nodiscard]] std::vector<std::filesystem::path> ChildDirectories(const std::filesystem::path& path) const override
    {
        std::vector<std::filesystem::path> children;

        std::error_code error;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(
                 path, std::filesystem::directory_options::skip_permission_denied, error))
        {
            const std::filesystem::file_status status = std::filesystem::symlink_status(entry.path(), error);

            if (status.type() == std::filesystem::file_type::directory || IsALinkWithoutFollowing(status))
            {
                children.push_back(entry.path());
            }
        }

        return children;
    }

    [[nodiscard]] bool VolumeIsAvailable(const std::filesystem::path&) const override
    {
        return true;
    }

    [[nodiscard]] bool ProbeWritable(const std::filesystem::path& path) const override
    {
        return TargetDirectoryExists(path);
    }

    [[nodiscard]] std::optional<std::uintmax_t> FreeSpaceOn(const std::filesystem::path& path) const override
    {
        std::error_code error;
        const std::filesystem::space_info space = std::filesystem::space(path, error);

        return error ? std::nullopt : std::optional(space.available);
    }

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point>
    LastWriteTime(const std::filesystem::path& path) const override
    {
        std::error_code error;
        const std::filesystem::file_time_type written = std::filesystem::last_write_time(path, error);

        return error ? std::nullopt : std::optional(SystemTimeOf(written));
    }

    [[nodiscard]] std::optional<std::vector<FileFingerprint>>
    FingerprintTree(const std::filesystem::path& root) const override
    {
        std::error_code error;
        std::filesystem::recursive_directory_iterator entry(
            root, std::filesystem::directory_options::skip_permission_denied, error);
        if (error)
        {
            return std::nullopt;
        }

        std::vector<FileFingerprint> files;
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
                const std::uintmax_t size = entry->file_size(error);
                if (error)
                {
                    return std::nullopt;
                }

                files.push_back(FileFingerprint{.relativePath = entry->path().lexically_relative(root), .size = size});
            }

            entry.increment(error);
            if (error)
            {
                return std::nullopt;
            }
        }

        return files;
    }
};

#endif // FS_ORGANIZER_TESTS_SUPPORT_STD_FILESYSTEM_PROBE_H
