#ifndef FS_ORGANIZER_DOMAIN_PORTS_FILE_OPERATIONS_H
#define FS_ORGANIZER_DOMAIN_PORTS_FILE_OPERATIONS_H

#include <filesystem>
#include <functional>
#include <string>

#include "domain/model/CopyOutcome.h"

class FileOperations
{
public:
    virtual ~FileOperations() = default;

    [[nodiscard]] virtual CopyOutcome CopyTree(const std::filesystem::path& source,
                                               const std::filesystem::path& destination,
                                               const std::function<bool(const CopyProgress&)>& onProgress) = 0;

    [[nodiscard]] virtual bool CreateFolder(const std::filesystem::path& path) = 0;

    [[nodiscard]] virtual bool WriteHiddenFile(const std::filesystem::path& path) = 0;

    [[nodiscard]] virtual bool Move(const std::filesystem::path& source, const std::filesystem::path& destination) = 0;

    [[nodiscard]] virtual bool RemoveTree(const std::filesystem::path& path) = 0;

    [[nodiscard]] virtual bool Recycle(const std::filesystem::path& path) = 0;

    [[nodiscard]] virtual bool RemoveEmptyFolder(const std::filesystem::path& path) = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_FILE_OPERATIONS_H
