#include "domain/tree/DestinationDivergence.h"

#include <algorithm>
#include <map>
#include <ranges>
#include <string>

#include "domain/linking/EntryClassifier.h"
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

std::vector<std::filesystem::path> DestinationsItStrayedTo(const SimulatorProfile& profile,
                                                           const std::vector<DestinationEntry>& entries,
                                                           const std::vector<std::filesystem::path>& addonFolders)
{
    std::multimap<std::string, const DestinationEntry*> linksByTarget;

    for (const DestinationEntry& entry : entries)
    {
        if (CountsAsEnabled(entry.classification))
        {
            linksByTarget.emplace(ComparablePath(entry.target), &entry);
        }
    }

    std::vector<std::filesystem::path> strayed;
    strayed.reserve(addonFolders.size());

    for (const std::filesystem::path& addonFolder : addonFolders)
    {
        const std::string wanted = ComparablePath(EffectiveDestination(profile, addonFolder));
        const auto [first, last] = linksByTarget.equal_range(ComparablePath(addonFolder));

        std::filesystem::path where;

        for (auto link = first; link != last; ++link)
        {
            if (ComparablePath(link->second->path.parent_path()) != wanted)
            {
                where = link->second->path.parent_path();
                break;
            }
        }

        strayed.push_back(std::move(where));
    }

    return strayed;
}

DestinationAgreement WhereTheEnabledAddonsPoint(const TreeNode& category, const std::vector<DestinationEntry>& entries)
{
    DestinationAgreement agreement{.destination = {}, .unanimous = true};

    for (const TreeNode* addon : AddonsUnder(category))
    {
        for (const std::filesystem::path& link : LinksPointingAt(entries, addon->path))
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
