#ifndef FS_ORGANIZER_DOMAIN_MODEL_DESTINATION_ENTRY_H
#define FS_ORGANIZER_DOMAIN_MODEL_DESTINATION_ENTRY_H

#include <filesystem>

enum class EntryClassification : int
{
    Managed = 0,
    External = 1,
    Broken = 2,
    Unavailable = 3,
    Unmanaged = 4,
    Duplicated = 5,
};

[[nodiscard]] constexpr bool CountsAsEnabled(const EntryClassification classification)
{
    return classification == EntryClassification::Managed || classification == EntryClassification::Duplicated;
}

struct DestinationEntry
{
    std::filesystem::path path;
    std::filesystem::path target;
    EntryClassification classification = EntryClassification::Unmanaged;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_DESTINATION_ENTRY_H
