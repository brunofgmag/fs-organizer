#ifndef FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_ARCHIVE_OR_FOLDER_READER_H
#define FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_ARCHIVE_OR_FOLDER_READER_H

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "domain/ports/ArchiveReader.h"

class ArchiveOrFolderReader final : public ArchiveReader
{
public:
    ArchiveOrFolderReader(const ArchiveReader& folders, const ArchiveReader& archives);

    [[nodiscard]] ArchiveListing EntriesOf(const std::filesystem::path& source) const override;

    [[nodiscard]] ArchiveText
    ContentsOf(const std::filesystem::path& source, const std::string& entry, std::size_t most) const override;

    [[nodiscard]] ArchiveFailure ExtractAll(const std::filesystem::path& source,
                                            const std::vector<ArchiveEntry>& listed,
                                            const std::filesystem::path& into,
                                            const std::function<bool(const CopyProgress&)>& onProgress) const override;

private:
    [[nodiscard]] const ArchiveReader& ReaderOf(const std::filesystem::path& source) const;

    const ArchiveReader& folders_;
    const ArchiveReader& archives_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_ARCHIVE_OR_FOLDER_READER_H
