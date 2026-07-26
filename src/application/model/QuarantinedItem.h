#ifndef FS_ORGANIZER_APPLICATION_MODEL_QUARANTINED_ITEM_H
#define FS_ORGANIZER_APPLICATION_MODEL_QUARANTINED_ITEM_H

#include <chrono>
#include <filesystem>
#include <optional>

struct QuarantinedItem
{
    std::filesystem::path path;
    std::filesystem::path origin;
    std::optional<std::chrono::system_clock::time_point> quarantinedAt;

    [[nodiscard]] bool KnowsWhereItCameFrom() const
    {
        return !origin.empty();
    }
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_QUARANTINED_ITEM_H
