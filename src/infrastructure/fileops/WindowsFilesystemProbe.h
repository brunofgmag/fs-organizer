#ifndef FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_WINDOWS_FILESYSTEM_PROBE_H
#define FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_WINDOWS_FILESYSTEM_PROBE_H

#include "domain/ports/FilesystemProbe.h"

class WindowsFilesystemProbe final : public FilesystemProbe
{
public:
    [[nodiscard]] bool EntryExistsWithoutFollowingLinks(const std::filesystem::path& path) const override;

    [[nodiscard]] bool TargetDirectoryExists(const std::filesystem::path& path) const override;

    [[nodiscard]] std::vector<std::filesystem::path> ChildDirectories(const std::filesystem::path& path) const override;

    [[nodiscard]] bool VolumeIsAvailable(const std::filesystem::path& path) const override;

    [[nodiscard]] bool ProbeWritable(const std::filesystem::path& path) const override;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_WINDOWS_FILESYSTEM_PROBE_H
