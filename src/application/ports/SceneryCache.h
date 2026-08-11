#ifndef FS_ORGANIZER_APPLICATION_PORTS_SCENERY_CACHE_H
#define FS_ORGANIZER_APPLICATION_PORTS_SCENERY_CACHE_H

#include <chrono>
#include <filesystem>
#include <optional>
#include <vector>

#include "domain/model/SceneryCodes.h"

struct RememberedScenery
{
    std::chrono::system_clock::time_point readAt{};
    std::vector<SceneryCodes> files{};
};

class SceneryCache
{
public:
    virtual ~SceneryCache() = default;

    [[nodiscard]] virtual std::optional<RememberedScenery> Remember(const std::filesystem::path& addonFolder) const = 0;

    virtual void Keep(const std::filesystem::path& addonFolder, const RememberedScenery& scenery) = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_SCENERY_CACHE_H
