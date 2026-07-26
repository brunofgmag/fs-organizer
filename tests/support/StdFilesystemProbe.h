#ifndef FS_ORGANIZER_TESTS_SUPPORT_STD_FILESYSTEM_PROBE_H
#define FS_ORGANIZER_TESTS_SUPPORT_STD_FILESYSTEM_PROBE_H

#include <system_error>

#include "domain/ports/FilesystemProbe.h"

class StdFilesystemProbe final : public FilesystemProbe
{
public:
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

        return std::filesystem::is_symlink(std::filesystem::symlink_status(path, error));
    }

    [[nodiscard]] std::vector<std::filesystem::path> ChildDirectories(
        const std::filesystem::path& path) const override
    {
        std::vector<std::filesystem::path> children;

        std::error_code error;
        for (const std::filesystem::directory_entry& entry :
             std::filesystem::directory_iterator(path, std::filesystem::directory_options::skip_permission_denied,
                                                 error))
        {
            if (entry.is_directory(error) || entry.is_symlink(error))
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

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> LastWriteTime(
        const std::filesystem::path& path) const override
    {
        std::error_code error;
        const std::filesystem::file_time_type written = std::filesystem::last_write_time(path, error);

        return error ? std::nullopt
                     : std::optional(std::chrono::system_clock::now()
                         + std::chrono::duration_cast<std::chrono::system_clock::duration>(
                             written - std::filesystem::file_time_type::clock::now()));
    }

    [[nodiscard]] std::vector<FileFingerprint> FingerprintTree(const std::filesystem::path& root) const override
    {
        std::vector<FileFingerprint> files;

        std::error_code error;
        for (const std::filesystem::directory_entry& entry :
             std::filesystem::recursive_directory_iterator(
                 root, std::filesystem::directory_options::skip_permission_denied, error))
        {
            if (entry.is_regular_file(error))
            {
                files.push_back(FileFingerprint{entry.path().lexically_relative(root), entry.file_size(error)});
            }
        }

        return files;
    }
};

#endif // FS_ORGANIZER_TESTS_SUPPORT_STD_FILESYSTEM_PROBE_H
