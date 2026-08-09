#ifndef FS_ORGANIZER_DOMAIN_MODEL_DESTINATION_ENTRY_H
#define FS_ORGANIZER_DOMAIN_MODEL_DESTINATION_ENTRY_H

#include <array>
#include <cstddef>
#include <filesystem>

#include "domain/support/PathUtils.h"

enum class EntryClassification : int
{
    Managed = 0,
    External = 1,
    Divergent = 2,
    Vanished = 3,
    Broken = 4,
    Unavailable = 5,
    Unmanaged = 6,
    Duplicated = 7,
};

inline constexpr std::array kEveryClassification = {EntryClassification::Managed,   EntryClassification::External,
                                                    EntryClassification::Divergent, EntryClassification::Vanished,
                                                    EntryClassification::Broken,    EntryClassification::Unavailable,
                                                    EntryClassification::Unmanaged, EntryClassification::Duplicated};

[[nodiscard]] constexpr std::size_t OrderOf(const EntryClassification classification)
{
    switch (classification)
    {
    case EntryClassification::Managed: return 0;
    case EntryClassification::External: return 1;
    case EntryClassification::Divergent: return 2;
    case EntryClassification::Vanished: return 3;
    case EntryClassification::Broken: return 4;
    case EntryClassification::Unavailable: return 5;
    case EntryClassification::Unmanaged: return 6;
    case EntryClassification::Duplicated: return 7;
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
    return classification == EntryClassification::Managed || classification == EntryClassification::Duplicated
        || classification == EntryClassification::Divergent;
}

struct DestinationEntry
{
    std::filesystem::path path{};
    std::filesystem::path target{};
    EntryClassification classification = EntryClassification::Unmanaged;
    std::filesystem::path externalOrigin{};
    std::filesystem::path libraryCopy{};
    bool theOtherProgramTookItsFolderBack = false;
};

[[nodiscard]] inline bool ItPointsAtTheOtherProgramsFolder(const DestinationEntry& entry)
{
    return !entry.externalOrigin.empty() && ComparablePath(entry.target) == ComparablePath(entry.externalOrigin);
}

#endif // FS_ORGANIZER_DOMAIN_MODEL_DESTINATION_ENTRY_H
