#ifndef FS_ORGANIZER_DOMAIN_PORTS_FILE_OPERATIONS_H
#define FS_ORGANIZER_DOMAIN_PORTS_FILE_OPERATIONS_H

#include <filesystem>

class FileOperations
{
public:
    virtual ~FileOperations() = default;

    [[nodiscard]] virtual bool EntryExistsWithoutFollowingLinks(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual bool TargetDirectoryExists(const std::filesystem::path& path) const = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_FILE_OPERATIONS_H
