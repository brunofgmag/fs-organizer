#ifndef FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_WINDOWS_FILE_OPERATIONS_H
#define FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_WINDOWS_FILE_OPERATIONS_H

#include "domain/ports/FileOperations.h"

class WindowsFileOperations final : public FileOperations
{
public:
    [[nodiscard]] bool EntryExistsWithoutFollowingLinks(const std::filesystem::path& path) const override;

    [[nodiscard]] bool TargetDirectoryExists(const std::filesystem::path& path) const override;

    [[nodiscard]] std::vector<std::filesystem::path> ChildDirectories(const std::filesystem::path& path) const override;

    [[nodiscard]] bool VolumeIsAvailable(const std::filesystem::path& path) const override;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_WINDOWS_FILE_OPERATIONS_H
