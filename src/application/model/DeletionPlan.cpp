#include "application/model/DeletionPlan.h"

#include <algorithm>

#include "domain/model/RecycleLimits.h"
#include "domain/support/PathUtils.h"

namespace
{
    bool TheAddonIsWithinReach(const AddonToDelete& addon)
    {
        return TheRecycleBinReaches(addon.longestEntry);
    }
}

std::filesystem::path VolumeOf(const std::filesystem::path& path)
{
    const std::string text = WithGenericSeparators(AsUtf8(path));

    if (text.size() >= 2 && text[1] == ':')
    {
        return PathFromUtf8(text.substr(0, 2));
    }

    return PathFromUtf8("/");
}

bool TheVolumeCanTake(const VolumeRoom& room)
{
    return room.itRecycles && room.selected.has_value() && room.quota.has_value() && *room.selected <= *room.quota;
}

const VolumeRoom* VolumeHolding(const DeletionPlan& plan, const std::filesystem::path& folder)
{
    const std::string wanted = ComparablePath(VolumeOf(folder));

    const auto found = std::ranges::find_if(plan.volumes,
                                            [&wanted](const VolumeRoom& room)
                                            {
                                                return ComparablePath(room.volume) == wanted;
                                            });

    return found == plan.volumes.end() ? nullptr : &*found;
}

bool TheRecycleBinCanTake(const DeletionPlan& plan)
{
    if (plan.addons.empty())
    {
        return false;
    }

    return std::ranges::all_of(plan.addons, TheAddonIsWithinReach)
        && std::ranges::all_of(plan.volumes, TheVolumeCanTake);
}
