#ifndef FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_WINDOWS_FILE_OPERATIONS_H
#define FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_WINDOWS_FILE_OPERATIONS_H

#include "domain/ports/FileOperations.h"

class WindowsFileOperations final : public FileOperations
{
public:
    [[nodiscard]] CopyOutcome CopyTree(const std::filesystem::path& source,
                                       const std::filesystem::path& destination,
                                       const std::function<bool(const CopyProgress&)>& onProgress) override;

    [[nodiscard]] bool CreateFolder(const std::filesystem::path& path) override;

    [[nodiscard]] bool WriteHiddenFile(const std::filesystem::path& path) override;

    [[nodiscard]] bool WriteTextFile(const std::filesystem::path& path, const std::string& contents) override;

    [[nodiscard]] bool Move(const std::filesystem::path& source, const std::filesystem::path& destination) override;

    [[nodiscard]] bool RemoveTree(const std::filesystem::path& path) override;

    [[nodiscard]] bool Recycle(const std::filesystem::path& path) override;

    [[nodiscard]] bool RemoveEmptyFolder(const std::filesystem::path& path) override;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_WINDOWS_FILE_OPERATIONS_H
