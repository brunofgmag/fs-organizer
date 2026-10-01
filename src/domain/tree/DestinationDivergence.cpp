#include "domain/tree/DestinationDivergence.h"

#include <string>

#include "domain/linking/EntryClassifier.h"
#include "domain/linking/LinksByTarget.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/EffectiveDestination.h"

std::filesystem::path DestinationItStrayedTo(const SimulatorProfile& profile,
                                             const std::vector<DestinationEntry>& entries,
                                             const std::filesystem::path& addonFolder)
{
    const std::string wanted = ComparablePath(EffectiveDestination(profile, addonFolder));

    for (const std::filesystem::path& link : LinksPointingAt(entries, addonFolder))
    {
        if (ComparablePath(link.parent_path()) != wanted)
        {
            return link.parent_path();
        }
    }

    return {};
}

DestinationAgreement WhereTheEnabledAddonsPoint(const TreeNode& category, const std::vector<DestinationEntry>& entries)
{
    DestinationAgreement agreement{.destination = {}, .unanimous = true};

    const LinksByTarget linksByTarget(entries);

    for (const TreeNode* addon : AddonsUnder(category))
    {
        for (const std::filesystem::path& link : linksByTarget.PointingAt(addon->path))
        {
            const std::filesystem::path where = link.parent_path();

            if (agreement.destination.empty())
            {
                agreement.destination = where;
                continue;
            }

            if (ComparablePath(agreement.destination) != ComparablePath(where))
            {
                return {};
            }
        }
    }

    return agreement;
}
