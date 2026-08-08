#ifndef FS_ORGANIZER_APPLICATION_MODEL_QUARANTINED_ITEM_H
#define FS_ORGANIZER_APPLICATION_MODEL_QUARANTINED_ITEM_H

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

#include "domain/model/QuarantineOrigin.h"
#include "domain/support/PathUtils.h"

struct QuarantinedItem
{
    std::filesystem::path path{};
    std::filesystem::path origin{};
    std::optional<std::chrono::system_clock::time_point> quarantinedAt{};
    OriginSource source = OriginSource::Unknown;
    std::filesystem::path theOtherSourceSays{};

    [[nodiscard]] bool KnowsWhereItCameFrom() const
    {
        return !origin.empty();
    }

    [[nodiscard]] bool TheSourcesDisagree() const
    {
        return !theOtherSourceSays.empty() && ComparablePath(theOtherSourceSays) != ComparablePath(origin);
    }

    [[nodiscard]] bool TheSourcesAgree() const
    {
        return !theOtherSourceSays.empty() && !TheSourcesDisagree();
    }
};

struct QuarantineDetail
{
    std::filesystem::path path{};
    std::string version{};
    std::filesystem::path replacedBy{};
    std::string replacementVersion{};

    [[nodiscard]] bool WasReplaced() const
    {
        return !replacedBy.empty();
    }
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_QUARANTINED_ITEM_H
