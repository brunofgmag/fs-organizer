#ifndef FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_TAR_EXE_ARCHIVE_READER_H
#define FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_TAR_EXE_ARCHIVE_READER_H

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <QtCore/QString>
#include <QtCore/QStringList>

#include "domain/ports/ArchiveReader.h"

inline constexpr std::chrono::milliseconds kTarTimeLimit{60000};
inline constexpr std::chrono::milliseconds kTarIdleLimit{120000};
inline constexpr qsizetype kMostLinesChecked = 64;

[[nodiscard]] std::filesystem::path SystemTarTool();

[[nodiscard]] bool AsksForAPassword(const QString& message, const QStringList& names);

[[nodiscard]] ArchiveListing ListingFrom(const QString& names, const QString& details);

class TarExeArchiveReader final : public ArchiveReader
{
public:
    explicit TarExeArchiveReader(std::filesystem::path tool = SystemTarTool(),
                                 std::chrono::milliseconds timeLimit = kTarTimeLimit,
                                 std::chrono::milliseconds idleLimit = kTarIdleLimit);

    [[nodiscard]] ArchiveListing EntriesOf(const std::filesystem::path& archive) const override;

    [[nodiscard]] ArchiveText
    ContentsOf(const std::filesystem::path& archive, const std::string& entry, std::size_t most) const override;

    [[nodiscard]] ArchiveFailure ExtractAll(const std::filesystem::path& archive,
                                            const std::vector<ArchiveEntry>& listed,
                                            const std::filesystem::path& into,
                                            const std::function<bool(const CopyProgress&)>& onProgress) const override;

private:
    std::filesystem::path tool_;
    std::chrono::milliseconds timeLimit_;
    std::chrono::milliseconds idleLimit_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_TAR_EXE_ARCHIVE_READER_H
