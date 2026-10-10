#ifndef FS_ORGANIZER_DOMAIN_PORTS_ARCHIVE_READER_H
#define FS_ORGANIZER_DOMAIN_PORTS_ARCHIVE_READER_H

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "domain/model/ArchiveEntry.h"
#include "domain/model/CopyOutcome.h"

class ArchiveReader
{
public:
    virtual ~ArchiveReader() = default;

    [[nodiscard]] virtual ArchiveListing EntriesOf(const std::filesystem::path& archive) const = 0;

    [[nodiscard]] virtual ArchiveText
    ContentsOf(const std::filesystem::path& archive, const std::string& entry, std::size_t most) const = 0;

    [[nodiscard]] virtual ArchiveFailure
    ExtractAll(const std::filesystem::path& archive,
               const std::vector<ArchiveEntry>& listed,
               const std::filesystem::path& into,
               const std::function<bool(const CopyProgress&)>& onProgress) const = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_ARCHIVE_READER_H
