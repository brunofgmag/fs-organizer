#include "domain/linking/RepairPlan.h"

#include <string>

#include "domain/model/AddonId.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    std::optional<std::filesystem::path> ValidAddonNamed(const std::vector<TreeNode>& libraries,
                                                         const std::string& baseName)
    {
        const TreeNode* addon = AddonNamed(libraries, baseName);

        return addon == nullptr ? std::nullopt : std::optional(addon->path);
    }
}

std::vector<RepairCandidate> PlanRepairs(const SimulatorProfile& profile,
                                         const std::vector<DestinationEntry>& entries,
                                         const std::vector<TreeNode>& libraries)
{
    std::vector<RepairCandidate> plan;

    for (const DestinationEntry& entry : entries)
    {
        if (entry.classification != EntryClassification::Broken)
        {
            continue;
        }

        RepairCandidate candidate;
        candidate.entry = entry;
        candidate.targetsLibrary = LibraryContaining(profile, entry.target) != nullptr;
        candidate.repointTo = ValidAddonNamed(libraries, entry.path.filename().string());

        plan.push_back(std::move(candidate));
    }

    return plan;
}
