#ifndef FS_ORGANIZER_DOMAIN_MODEL_DESTINATION_ENTRY_H
#define FS_ORGANIZER_DOMAIN_MODEL_DESTINATION_ENTRY_H

#include <array>
#include <cstddef>
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

inline constexpr std::array kEveryClassification = {EntryClassification::Managed,   EntryClassification::External,
                                                    EntryClassification::Broken,    EntryClassification::Unavailable,
                                                    EntryClassification::Unmanaged, EntryClassification::Duplicated};

[[nodiscard]] constexpr std::size_t OrderOf(const EntryClassification classification)
{
    switch (classification)
    {
    case EntryClassification::Managed: return 0;
    case EntryClassification::External: return 1;
    case EntryClassification::Broken: return 2;
    case EntryClassification::Unavailable: return 3;
    case EntryClassification::Unmanaged: return 4;
    case EntryClassification::Duplicated: return 5;
    }

    return kEveryClassification.size();
}

[[nodiscard]] constexpr bool EveryClassificationIsListed()
{
    for (std::size_t order = 0; order < kEveryClassification.size(); ++order)
    {
        if (OrderOf(kEveryClassification[order]) != order)
        {
            return false;
        }
    }

    return true;
}

static_assert(EveryClassificationIsListed());

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
