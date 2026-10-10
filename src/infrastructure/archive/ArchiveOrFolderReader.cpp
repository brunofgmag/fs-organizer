#include "infrastructure/archive/ArchiveOrFolderReader.h"

#include <system_error>

#include "infrastructure/fileops/ExtendedPaths.h"

ArchiveOrFolderReader::ArchiveOrFolderReader(const ArchiveReader& folders, const ArchiveReader& archives)
    : folders_(folders), archives_(archives)
{
}

const ArchiveReader& ArchiveOrFolderReader::ReaderOf(const std::filesystem::path& source) const
{
    std::error_code error;

    return std::filesystem::is_directory(WithExtendedPrefix(source), error) ? folders_ : archives_;
}

ArchiveListing ArchiveOrFolderReader::EntriesOf(const std::filesystem::path& source) const
{
    return ReaderOf(source).EntriesOf(source);
}

ArchiveText ArchiveOrFolderReader::ContentsOf(const std::filesystem::path& source,
                                              const std::string& entry,
                                              const std::size_t most) const
{
    return ReaderOf(source).ContentsOf(source, entry, most);
}

ArchiveFailure ArchiveOrFolderReader::ExtractAll(const std::filesystem::path& source,
                                                 const std::vector<ArchiveEntry>& listed,
                                                 const std::filesystem::path& into,
                                                 const std::function<bool(const CopyProgress&)>& onProgress) const
{
    return ReaderOf(source).ExtractAll(source, listed, into, onProgress);
}
