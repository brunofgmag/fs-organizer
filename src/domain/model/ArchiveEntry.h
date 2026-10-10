#ifndef FS_ORGANIZER_DOMAIN_MODEL_ARCHIVE_ENTRY_H
#define FS_ORGANIZER_DOMAIN_MODEL_ARCHIVE_ENTRY_H

#include <cstdint>
#include <string>
#include <vector>

enum class ArchiveFailure : int
{
    None = 0,
    TheToolIsMissing = 1,
    CouldNotOpen = 2,
    NeedsAPassword = 3,
    CouldNotExtract = 4,
    TookTooLong = 5,
    Cancelled = 6,
};

enum class ArchiveEntryKind : int
{
    File = 0,
    Folder = 1,
    Other = 2,
};

struct ArchiveEntry
{
    std::string path{};
    std::uintmax_t bytes = 0;
    ArchiveEntryKind kind = ArchiveEntryKind::File;
};

struct ArchiveListing
{
    ArchiveFailure failure = ArchiveFailure::None;
    std::vector<ArchiveEntry> entries{};
};

struct ArchiveText
{
    ArchiveFailure failure = ArchiveFailure::None;
    std::string text{};
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_ARCHIVE_ENTRY_H
