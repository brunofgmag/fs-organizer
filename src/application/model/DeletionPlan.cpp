#include "application/model/DeletionPlan.h"

#include <algorithm>

#include "domain/model/RecycleLimits.h"
#include "domain/support/PathUtils.h"

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

FileResult WhatTheRecycleBinRefuses(const DeletionPlan& plan, const AddonToDelete& addon)
{
    const VolumeRoom* room = VolumeHolding(plan, addon.folder);

    if (room == nullptr || !TheVolumeCanTake(*room))
    {
        return FileResult::TheRecycleBinIsTooSmall;
    }

    if (!TheRecycleBinReaches(addon.longestEntry))
    {
        return FileResult::TheRecycleBinCannotReachIt;
    }

    return FileResult::Completed;
}

bool TheRecycleBinCanTake(const DeletionPlan& plan)
{
    return !plan.addons.empty() && AddonsTheRecycleBinRefuses(plan) == 0;
}

std::size_t AddonsTheRecycleBinRefuses(const DeletionPlan& plan)
{
    return static_cast<std::size_t>(std::ranges::count_if(plan.addons,
                                                          [&plan](const AddonToDelete& addon)
                                                          {
                                                              return !Succeeded(WhatTheRecycleBinRefuses(plan, addon));
                                                          }));
}

bool EveryAddonCameFromAnotherProgram(const DeletionPlan& plan)
{
    return !plan.addons.empty()
        && std::ranges::all_of(plan.addons,
                               [](const AddonToDelete& addon)
                               {
                                   return !addon.cameFrom.empty();
                               });
}
