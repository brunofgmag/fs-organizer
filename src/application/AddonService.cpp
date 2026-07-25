#include "application/AddonService.h"

#include <set>
#include <string>
#include <utility>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/EffectiveDestination.h"
#include "domain/tree/LibraryLookup.h"

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

    AddonId IdentityOf(const SimulatorProfile& profile, const std::filesystem::path& addonFolder)
    {
        const Library* library = LibraryContaining(profile, addonFolder);

        return AddonId{library == nullptr ? LibraryId{} : library->id, addonFolder.filename().string()};
    }
}

AddonService::AddonService(const CatalogScanner& catalog,
                           const EnabledStateResolver& resolver,
                           const LinkingEngine& linking,
                           OperationJournal& journal,
                           const Clock& clock,
                           const LibraryIdGenerator& identities,
                           const LinkType linkType)
    : catalog_(catalog),
      resolver_(resolver),
      linking_(linking),
      journal_(journal),
      clock_(clock),
      identities_(identities),
      linkType_(linkType)
{
}

LibraryReport AddonService::RegisterLibrary(SimulatorProfile& profile,
                                            const std::filesystem::path& path) const
{
    const TreeNode tree = catalog_.Scan(path);

    LibraryReport report;
    report.accepted = LibraryContaining(profile, path) == nullptr;
    report.categories = tree.children.size();
    report.addons = CountAddons(tree);

    if (report.accepted)
    {
        profile.libraries.push_back(Library{identities_.Generate(), path, path.filename().string()});
    }

    return report;
}

TreeSnapshot AddonService::Scan(const SimulatorProfile& profile) const
{
    TreeSnapshot snapshot;

    for (const Library& library : profile.libraries)
    {
        snapshot.libraries.push_back(catalog_.Scan(library.path));
    }

    snapshot.entries = ResolveEntries(profile);
    snapshot.enabled = EnabledAddons(EnabledAddonFolders(snapshot.entries));

    return snapshot;
}

std::vector<DestinationEntry> AddonService::ResolveEntries(const SimulatorProfile& profile) const
{
    return resolver_.Resolve(profile.destinations, LibraryRoots(profile));
}

std::vector<AddonService::Step> AddonService::PlanSteps(const SimulatorProfile& profile,
                                                        const TreeSnapshot& snapshot,
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

std::vector<AddonService::Step> AddonService::StepsFor(
    const SimulatorProfile& profile,
    const std::multimap<std::string, const DestinationEntry*>& linksByTarget,
    const TreeNode& addon,
    const bool enable)
{
    const AddonId identity = IdentityOf(profile, addon.path);

    if (enable)
    {
        const std::filesystem::path destination = EffectiveDestination(profile, addon.path);

        return {
            {
                OperationKind::EnableAddon, identity, addon.path,
                destination / addon.path.filename()
            }
        };
    }

    std::vector<Step> steps;
    const auto [first, last] = linksByTarget.equal_range(ComparablePath(addon.path));
    for (auto entry = first; entry != last; ++entry)
    {
        steps.push_back({OperationKind::DisableAddon, identity, addon.path, entry->second->path});
    }

    return steps;
}

AddonService::Step AddonService::Inverse(const Step& step)
{
    return {
        step.kind == OperationKind::EnableAddon
            ? OperationKind::DisableAddon
            : OperationKind::EnableAddon,
        step.addonId, step.addonFolder, step.linkPath
    };
}

LinkOperationResult AddonService::Run(const Step& step) const
{
    const LinkOutcome outcome = step.kind == OperationKind::EnableAddon
                                    ? linking_.Enable(Addon{step.addonFolder, Manifest{}},
                                                      step.linkPath.parent_path(), linkType_)
                                    : linking_.Disable(step.linkPath);

    journal_.Append(OperationRecord{
        clock_.Now(), step.kind, step.addonId, step.addonFolder,
        step.linkPath, outcome.Failure()
    });

    return LinkOperationResult{step.addonId, step.addonFolder, step.linkPath, step.kind, outcome};
}

std::vector<LinkOperationResult> AddonService::SetEnabled(const SimulatorProfile& profile,
                                                          const TreeSnapshot& snapshot,
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

bool AddonService::CanUndo() const
{
    return !undo_.empty();
}

std::vector<LinkOperationResult> AddonService::UndoLastBatch()
{
    const std::vector<Step> steps = std::exchange(undo_, {});

    std::vector<LinkOperationResult> results;
    for (const Step& step : steps)
    {
        results.push_back(Run(step));
    }

    return results;
}
