#include "application/ProfileService.h"

#include <set>
#include <string>
#include <utility>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/EffectiveDestination.h"
#include "domain/tree/LibraryLookup.h"
#include "domain/tree/LibraryTrees.h"

namespace
{
    std::vector<std::filesystem::path> LibraryRoots(const SimulatorProfile& profile)
    {
        std::vector<std::filesystem::path> roots;
        for (const Library& library : profile.libraries)
        {
            roots.push_back(library.path);
        }

        return roots;
    }
}

ProfileService::ProfileService(const CatalogScanner& catalog,
                               const EntryClassifier& classifier,
                               const LinkingEngine& linking,
                               const OperationLog& log,
                               const LibraryIdGenerator& identities,
                               const LinkType linkType)
    : catalog_(catalog),
      classifier_(classifier),
      linking_(linking),
      log_(log),
      identities_(identities),
      linkType_(linkType)
{
}

LibraryReport ProfileService::RegisterLibrary(SimulatorProfile& profile, const std::filesystem::path& path) const
{
    const TreeNode tree = catalog_.Scan(path);

    LibraryReport report;
    report.check = LibraryContaining(profile, path) == nullptr ? LibraryCheck::Accepted
                                                               : LibraryCheck::RejectedInsideAnotherLibrary;
    report.categories = tree.children.size();
    report.addons = CountAddons(tree);

    if (report.Accepted())
    {
        profile.libraries.push_back(Library{identities_.Generate(), path, path.filename().string()});
    }

    return report;
}

ProfileSnapshot ProfileService::Scan(const SimulatorProfile& profile) const
{
    ProfileSnapshot snapshot;

    snapshot.libraries = LibraryTreesOf(catalog_, profile);
    snapshot.entries = ResolveEntries(profile);
    snapshot.enabled = EnabledAddons(EnabledAddonFolders(snapshot.entries));
    snapshot.conflicts = FindCopyConflicts(snapshot.entries, snapshot.libraries);

    return snapshot;
}

std::vector<DestinationEntry> ProfileService::ResolveEntries(const SimulatorProfile& profile) const
{
    return classifier_.Resolve(profile.destinations, LibraryRoots(profile));
}

std::vector<ProfileService::Step> ProfileService::PlanSteps(const SimulatorProfile& profile,
                                                            const ProfileSnapshot& snapshot,
                                                            const std::vector<const TreeNode*>& nodes,
                                                            const bool enable)
{
    std::vector<Step> steps;
    std::set<std::string> planned;

    std::multimap<std::string, const DestinationEntry*> linksByTarget;
    for (const DestinationEntry& entry : snapshot.entries)
    {
        if (entry.classification == EntryClassification::Managed
            || entry.classification == EntryClassification::Duplicated)
        {
            linksByTarget.emplace(ComparablePath(entry.target), &entry);
        }
    }

    for (const TreeNode* node : nodes)
    {
        for (const TreeNode* addon : AddonsUnder(*node))
        {
            if (!planned.insert(ComparablePath(addon->path)).second)
            {
                continue;
            }

            if (snapshot.enabled.Contains(addon->path) == enable)
            {
                continue;
            }

            const std::vector<Step> next = StepsFor(profile, linksByTarget, *addon, enable);
            steps.insert(steps.end(), next.begin(), next.end());
        }
    }

    return steps;
}

std::vector<ProfileService::Step>
ProfileService::StepsFor(const SimulatorProfile& profile,
                         const std::multimap<std::string, const DestinationEntry*>& linksByTarget,
                         const TreeNode& addon,
                         const bool enable)
{
    const AddonId identity = IdentityOf(profile, addon.path);

    if (enable)
    {
        return {{OperationKind::EnableAddon, identity, addon.path, PlannedLinkPath(profile, addon.path)}};
    }

    std::vector<Step> steps;
    const auto [first, last] = linksByTarget.equal_range(ComparablePath(addon.path));
    for (auto entry = first; entry != last; ++entry)
    {
        steps.push_back({OperationKind::DisableAddon, identity, addon.path, entry->second->path});
    }

    return steps;
}

ProfileService::Step ProfileService::Inverse(const Step& step)
{
    return {step.kind == OperationKind::EnableAddon ? OperationKind::DisableAddon : OperationKind::EnableAddon,
            step.addonId, step.addonFolder, step.linkPath};
}

LinkOperationResult ProfileService::Run(const Step& step) const
{
    const LinkOutcome outcome = CreatesALink(step.kind)
        ? linking_.Enable(Addon{step.addonFolder, Manifest{}}, step.linkPath.parent_path(), linkType_)
        : linking_.Disable(step.linkPath);

    log_.RecordLink(step.kind, step.addonId, step.addonFolder, step.linkPath,
                                            outcome.Failure());

    return LinkOperationResult{step.addonId, step.addonFolder, step.linkPath, step.kind, outcome};
}

std::vector<LinkOperationResult> ProfileService::SetEnabled(const SimulatorProfile& profile,
                                                            const ProfileSnapshot& snapshot,
                                                            const std::vector<const TreeNode*>& nodes,
                                                            const bool enable)
{
    const std::vector<Step> steps = PlanSteps(profile, snapshot, nodes, enable);

    std::vector<LinkOperationResult> results;
    std::vector<Step> undo;

    for (const Step& step : steps)
    {
        LinkOperationResult result = Run(step);

        if (result.outcome.Succeeded())
        {
            undo.push_back(Inverse(step));
        }

        results.push_back(std::move(result));
    }

    if (!undo.empty())
    {
        undo_ = std::move(undo);
    }

    return results;
}

std::optional<ProfileService::Step> ProfileService::PlanRepair(const SimulatorProfile& profile,
                                                               const RepairRequest& request)
{
    const DestinationEntry& entry = request.candidate.entry;

    if (request.action == RepairAction::Repoint)
    {
        if (!request.candidate.repointTo.has_value())
        {
            return std::nullopt;
        }

        return Step{OperationKind::RepointLink, IdentityOf(profile, *request.candidate.repointTo),
                    *request.candidate.repointTo, entry.path};
    }

    return Step{OperationKind::RemoveBrokenLink, IdentityOf(profile, entry.target), entry.target, entry.path};
}

std::vector<ProfileService::Step> ProfileService::Inverse(const SimulatorProfile& profile, const RepairRequest& request)
{
    const DestinationEntry& entry = request.candidate.entry;

    std::vector<Step> undo;

    if (request.action == RepairAction::Repoint)
    {
        undo.push_back({OperationKind::DisableAddon, IdentityOf(profile, *request.candidate.repointTo),
                        *request.candidate.repointTo, entry.path});
    }

    undo.push_back({OperationKind::EnableAddon, IdentityOf(profile, entry.target), entry.target, entry.path});

    return undo;
}

std::vector<LinkOperationResult> ProfileService::Repair(const SimulatorProfile& profile,
                                                        const std::vector<RepairRequest>& requests)
{
    std::vector<LinkOperationResult> results;
    std::vector<Step> undo;

    for (const RepairRequest& request : requests)
    {
        const std::optional<Step> step = PlanRepair(profile, request);
        if (!step.has_value())
        {
            continue;
        }

        LinkOperationResult result = Run(*step);

        if (result.outcome.Succeeded())
        {
            const std::vector<Step> inverse = Inverse(profile, request);
            undo.insert(undo.end(), inverse.begin(), inverse.end());
        }

        results.push_back(std::move(result));
    }

    if (!undo.empty())
    {
        undo_ = std::move(undo);
    }

    return results;
}

bool ProfileService::CanUndo() const
{
    return !undo_.empty();
}

void ProfileService::ForgetUndo()
{
    undo_.clear();
}

std::vector<LinkOperationResult> ProfileService::UndoLastBatch()
{
    const std::vector<Step> steps = std::exchange(undo_, {});

    std::vector<LinkOperationResult> results;
    for (const Step& step : steps)
    {
        results.push_back(Run(step));
    }

    return results;
}
