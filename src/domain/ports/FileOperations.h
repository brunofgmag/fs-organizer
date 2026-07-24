#ifndef FS_ORGANIZER_DOMAIN_PORTS_FILE_OPERATIONS_H
#define FS_ORGANIZER_DOMAIN_PORTS_FILE_OPERATIONS_H

#include <filesystem>
#include <vector>

class FileOperations
{
public:
    virtual ~FileOperations() = default;

    [[nodiscard]] virtual bool EntryExistsWithoutFollowingLinks(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual bool TargetDirectoryExists(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual std::vector<std::filesystem::path> ChildDirectories(
        const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual bool VolumeIsAvailable(const std::filesystem::path& path) const = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_FILE_OPERATIONS_H
