#include "infrastructure/archive/PosixFolderArchiveReader.h"

ArchiveListing PosixFolderArchiveReader::EntriesOf(const std::filesystem::path&) const
{
    return {.failure = ArchiveFailure::CouldNotOpen, .entries = {}};
}

ArchiveText PosixFolderArchiveReader::ContentsOf(const std::filesystem::path&, const std::string&, std::size_t) const
{
    return {.failure = ArchiveFailure::CouldNotOpen, .text = {}};
}

ArchiveFailure PosixFolderArchiveReader::ExtractAll(const std::filesystem::path&,
                                                    const std::vector<ArchiveEntry>&,
                                                    const std::filesystem::path&,
                                                    const std::function<bool(const CopyProgress&)>&) const
{
    return ArchiveFailure::CouldNotExtract;
}
