#include "domain/tree/ToggleDirection.h"

#include <map>
#include <string>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/EffectiveDestination.h"

namespace
{
    bool LinksTo(const DestinationEntry& entry, const std::filesystem::path& addonFolder)
    {
        return CountsAsEnabled(entry.classification) && ComparablePath(entry.target) == ComparablePath(addonFolder);
    }
}

bool DestinationBlocks(const SimulatorProfile& profile,
                       const std::vector<DestinationEntry>& entries,
                       const std::filesystem::path& addonFolder)
{
    const std::string linkPath = ComparablePath(PlannedLinkPath(profile, addonFolder));

    for (const DestinationEntry& entry : entries)
    {
        if (ComparablePath(entry.path) == linkPath && !LinksTo(entry, addonFolder))
        {
            return true;
        }
    }

    return false;
}

bool ShouldEnable(const SimulatorProfile& profile,
                  const std::vector<DestinationEntry>& entries,
                  const EnabledAddons& enabled,
                  const std::vector<const TreeNode*>& nodes)
{
    std::map<std::string, const DestinationEntry*> occupants;
    for (const DestinationEntry& entry : entries)
    {
        occupants.emplace(ComparablePath(entry.path), &entry);
    }

    const auto freeToEnable = [&profile, &occupants](const TreeNode& addon)
    {
        const auto occupant = occupants.find(ComparablePath(PlannedLinkPath(profile, addon.path)));

        return occupant == occupants.end() || LinksTo(*occupant->second, addon.path);
    };

    bool somethingToDisable = false;

    for (const TreeNode* node : nodes)
    {
        for (const TreeNode* addon : AddonsUnder(*node))
        {
            if (enabled.Contains(addon->path))
            {
                somethingToDisable = true;
                continue;
            }

            if (freeToEnable(*addon))
            {
                return true;
            }
        }
    }

    return !somethingToDisable;
}
