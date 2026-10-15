#include "infrastructure/archive/LibarchiveArchiveReader.h"

ArchiveListing LibarchiveArchiveReader::EntriesOf(const std::filesystem::path&) const
{
    return {.failure = ArchiveFailure::TheToolIsMissing, .entries = {}};
}

ArchiveText LibarchiveArchiveReader::ContentsOf(const std::filesystem::path&, const std::string&, std::size_t) const
{
    return {.failure = ArchiveFailure::TheToolIsMissing, .text = {}};
}

ArchiveFailure LibarchiveArchiveReader::ExtractAll(const std::filesystem::path&,
                                                   const std::vector<ArchiveEntry>&,
                                                   const std::filesystem::path&,
                                                   const std::function<bool(const CopyProgress&)>&) const
{
    return ArchiveFailure::TheToolIsMissing;
}
