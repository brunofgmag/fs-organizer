#ifndef FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_FOLDER_ARCHIVE_READER_H
#define FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_FOLDER_ARCHIVE_READER_H

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "domain/ports/ArchiveReader.h"
#include "infrastructure/archive/TarExeArchiveReader.h"

inline constexpr std::chrono::milliseconds kFolderTimeLimit = kTarTimeLimit;
inline constexpr std::chrono::milliseconds kFolderIdleLimit = kTarIdleLimit;

class FolderArchiveReader final : public ArchiveReader
{
public:
    explicit FolderArchiveReader(std::chrono::milliseconds timeLimit = kFolderTimeLimit,
                                 std::chrono::milliseconds idleLimit = kFolderIdleLimit);

    [[nodiscard]] ArchiveListing EntriesOf(const std::filesystem::path& folder) const override;

    [[nodiscard]] ArchiveText
    ContentsOf(const std::filesystem::path& folder, const std::string& entry, std::size_t most) const override;

    [[nodiscard]] ArchiveFailure ExtractAll(const std::filesystem::path& folder,
                                            const std::vector<ArchiveEntry>& listed,
                                            const std::filesystem::path& into,
                                            const std::function<bool(const CopyProgress&)>& onProgress) const override;

private:
    std::chrono::milliseconds timeLimit_;
    std::chrono::milliseconds idleLimit_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_FOLDER_ARCHIVE_READER_H
