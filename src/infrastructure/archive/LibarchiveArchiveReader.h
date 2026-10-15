#ifndef FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_LIBARCHIVE_ARCHIVE_READER_H
#define FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_LIBARCHIVE_ARCHIVE_READER_H

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "domain/ports/ArchiveReader.h"

inline constexpr std::chrono::milliseconds kLibarchiveTimeLimit{60000};
inline constexpr std::chrono::milliseconds kLibarchiveIdleLimit{120000};

class LibarchiveArchiveReader final : public ArchiveReader
{
public:
    explicit LibarchiveArchiveReader(std::chrono::milliseconds timeLimit = kLibarchiveTimeLimit,
                                     std::chrono::milliseconds idleLimit = kLibarchiveIdleLimit);

    [[nodiscard]] ArchiveListing EntriesOf(const std::filesystem::path& source) const override;

    [[nodiscard]] ArchiveText
    ContentsOf(const std::filesystem::path& source, const std::string& entry, std::size_t most) const override;

    [[nodiscard]] ArchiveFailure ExtractAll(const std::filesystem::path& source,
                                            const std::vector<ArchiveEntry>& listed,
                                            const std::filesystem::path& into,
                                            const std::function<bool(const CopyProgress&)>& onProgress) const override;

private:
    std::chrono::milliseconds timeLimit_;
    std::chrono::milliseconds idleLimit_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_LIBARCHIVE_ARCHIVE_READER_H
