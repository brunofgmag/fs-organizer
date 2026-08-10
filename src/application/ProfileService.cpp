#include "application/ProfileService.h"

#include <set>
#include <string>
#include <utility>

#include "domain/importing/ExternalSidecar.h"
#include "domain/profile/ExternalOrigins.h"
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

    std::map<std::string, const DestinationEntry*> LinksHeldByPath(const std::vector<DestinationEntry>& entries)
    {
        std::map<std::string, const DestinationEntry*> held;

        for (const DestinationEntry& entry : entries)
        {
            if (CountsAsEnabled(entry.classification))
            {
                held.emplace(ComparablePath(entry.path), &entry);
            }
        }

        return held;
    }
}

ProfileService::ProfileService(const CatalogScanner& catalog,
                               const FilesystemProbe& filesystemProbe,
                               const SidecarStore& sidecars,
                               const EntryClassifier& classifier,
                               const LinkingEngine& linking,
                               const OperationLog& log,
                               const LibraryIdGenerator& identities,
                               StartupService& startup,
                               const LinkType linkType)
    : catalog_(catalog),
      filesystemProbe_(filesystemProbe),
      sidecars_(sidecars),
      classifier_(classifier),
      linking_(linking),
      log_(log),
      identities_(identities),
      startup_(startup),
      linkType_(linkType)
{
}

void ProfileService::UseLinkType(const LinkType linkType)
{
    linkType_ = linkType;
}

std::vector<StartupLine> ProfileService::StartupEntriesCarriedBy(const SimulatorProfile& profile,
                                                                 const ProfileSnapshot& shown,
                                                                 const std::vector<const TreeNode*>& nodes) const
{
    std::vector<std::filesystem::path> folders;

    for (const TreeNode* node : nodes)
    {
        for (const TreeNode* addon : AddonsUnder(*node))
        {
            folders.push_back(addon->path);
        }
    }

    return EntriesCarriedBy(startup_.Report(profile, shown), folders);
}

LibraryReport ProfileService::RegisterLibrary(SimulatorProfile& profile, const std::filesystem::path& path) const
{
    const TreeNode tree = catalog_.Scan(path);

    LibraryReport report;
    report.check = LibraryContaining(profile, path) == nullptr ? LibraryCheck::Accepted
                                                               : LibraryCheck::RejectedInsideAnotherLibrary;
    report.categories = CountCategoriesInside(tree);
    report.addons = CountAddons(tree);

    if (report.Accepted())
    {
        profile.libraries.push_back(
            Library{.id = identities_.Generate(), .path = path, .label = AsUtf8(path.filename())});
    }

    return report;
}

ProfileSnapshot ProfileService::Scan(const SimulatorProfile& profile) const
{
    ProfileSnapshot snapshot;

    snapshot.libraries = LibraryTreesOf(catalog_, profile);
    snapshot.entries = ResolveEntries(profile, snapshot.libraries);
    snapshot.enabled = EnabledAddons(EnabledAddonFolders(snapshot.entries));
    snapshot.conflicts = FindCopyConflicts(snapshot.entries, snapshot.libraries);

    return snapshot;
}

std::vector<DestinationEntry> ProfileService::ResolveEntries(const SimulatorProfile& profile,
                                                             const std::vector<TreeNode>& libraries) const
{
    return classifier_.Resolve(profile.destinations, LibraryRoots(profile),
                               WhatCameFromAnotherProgram(profile, libraries));
}

std::vector<ExternalAddon> ProfileService::WhatCameFromAnotherProgram(const SimulatorProfile& profile,
                                                                      const std::vector<TreeNode>& libraries) const
{
    std::vector<ExternalAddon> known = ExternalAddonsOf(profile);

    for (const TreeNode& library : libraries)
    {
        for (const TreeNode* addon : AddonsUnder(library))
        {
            const std::optional<std::string> written = sidecars_.Read(ExternalSidecarPathFor(addon->path));
            if (!written.has_value())
            {
                continue;
            }

            if (const std::optional<std::filesystem::path> came = ExternalOriginFromText(*written); came.has_value())
            {
                RememberedByTheLibrary(known, addon->path, *came);
            }
        }
    }

    return known;
}

ProfileService::LinksOnDisk ProfileService::ReadLinksNow(const SimulatorProfile& profile) const
{
    std::vector<DestinationEntry> entries = ResolveEntries(profile);
    EnabledAddons enabled{EnabledAddonFolders(entries)};

    return {.entries = std::move(entries), .enabled = std::move(enabled)};
}

std::size_t ProfileService::AddonsThatDrifted(const std::vector<const TreeNode*>& nodes,
                                              const EnabledAddons& shown,
                                              const EnabledAddons& onDisk)
{
    std::set<std::string> drifted;

    for (const TreeNode* node : nodes)
    {
        for (const TreeNode* addon : AddonsUnder(*node))
        {
            if (shown.Contains(addon->path) != onDisk.Contains(addon->path))
            {
                drifted.insert(ComparablePath(addon->path));
            }
        }
    }

    return drifted.size();
}

std::vector<TakenPlace> ProfileService::PlacesTaken(const SimulatorProfile& profile,
                                                    const std::vector<const TreeNode*>& nodes) const
{
    const LinksOnDisk onDisk = ReadLinksNow(profile);
    const std::map<std::string, const DestinationEntry*> held = LinksHeldByPath(onDisk.entries);

    std::vector<TakenPlace> taken;
    std::set<std::string> asked;

    for (const TreeNode* node : nodes)
    {
        for (const TreeNode* addon : AddonsUnder(*node))
        {
            if (onDisk.enabled.Contains(addon->path) || !asked.insert(ComparablePath(addon->path)).second)
            {
                continue;
            }

            const std::filesystem::path place = PlannedLinkPath(profile, addon->path);
            const auto occupied = held.find(ComparablePath(place));

            if (occupied != held.end())
            {
                taken.push_back(
                    TakenPlace{.addonFolder = addon->path, .linkPath = place, .occupant = occupied->second->target});
            }
        }
    }

    return taken;
}

std::vector<ProfileService::Step> ProfileService::PlanSteps(const SimulatorProfile& profile,
                                                            const LinksOnDisk& onDisk,
                                                            const std::vector<const TreeNode*>& nodes,
                                                            const bool enable,
                                                            const std::vector<StartupLine>& startupEntriesToTurnOff)
{
    std::vector<Step> steps;
    std::set<std::string> planned;

    std::multimap<std::string, const DestinationEntry*> linksByTarget;
    for (const DestinationEntry& entry : onDisk.entries)
    {
        if (CountsAsEnabled(entry.classification))
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

            if (onDisk.enabled.Contains(addon->path) == enable)
            {
                continue;
            }

            const std::vector<Step> next = StepsFor(profile, linksByTarget, *addon, enable, startupEntriesToTurnOff);
            steps.insert(steps.end(), next.begin(), next.end());
        }
    }

    return steps;
}

std::vector<ProfileService::Step>
ProfileService::StepsFor(const SimulatorProfile& profile,
                         const std::multimap<std::string, const DestinationEntry*>& linksByTarget,
                         const TreeNode& addon,
                         const bool enable,
                         const std::vector<StartupLine>& startupEntriesToTurnOff)
{
    const AddonId identity = IdentityOf(profile, addon.path);

    if (enable)
    {
        return {{.kind = OperationKind::EnableAddon,
                 .addonId = identity,
                 .addonFolder = addon.path,
                 .linkPath = PlannedLinkPath(profile, addon.path)}};
    }

    std::vector<Step> steps;
    const auto [first, last] = linksByTarget.equal_range(ComparablePath(addon.path));
    for (auto entry = first; entry != last; ++entry)
    {
        steps.push_back({.kind = OperationKind::DisableAddon,
                         .addonId = identity,
                         .addonFolder = addon.path,
                         .linkPath = entry->second->path});
    }

    for (const StartupLine& line : EntriesCarriedBy(StartupReport{.lines = startupEntriesToTurnOff}, {addon.path}))
    {
        steps.push_back({.kind = OperationKind::TurnOffTheStartupEntry,
                         .addonId = identity,
                         .addonFolder = addon.path,
                         .linkPath = line.path,
                         .label = line.label});
    }

    return steps;
}

namespace
{
    OperationKind TheOppositeOf(const OperationKind kind)
    {
        switch (kind)
        {
        case OperationKind::EnableAddon: return OperationKind::DisableAddon;
        case OperationKind::TurnOffTheStartupEntry: return OperationKind::TurnOnTheStartupEntry;
        case OperationKind::TurnOnTheStartupEntry: return OperationKind::TurnOffTheStartupEntry;
        default: return OperationKind::EnableAddon;
        }
    }
}

ProfileService::Step ProfileService::Inverse(const Step& step)
{
    return {.kind = TheOppositeOf(step.kind),
            .addonId = step.addonId,
            .addonFolder = step.addonFolder,
            .linkPath = step.linkPath,
            .label = step.label};
}

LinkOperationResult ProfileService::RunTheStartupStep(const Step& step) const
{
    const bool turningOn = step.kind == OperationKind::TurnOnTheStartupEntry;
    const FileResult result = startup_.Switch(step.linkPath, turningOn);

    log_.RecordImport(step.kind, step.addonId, step.addonFolder, step.linkPath, result, OriginSource::Unknown,
                      step.label);

    return LinkOperationResult{.addonId = step.addonId,
                               .addonFolder = step.addonFolder,
                               .linkPath = step.linkPath,
                               .kind = step.kind,
                               .outcome = LinkOutcome::OfFile(result)};
}

LinkOperationResult ProfileService::Run(const Step& step) const
{
    if (step.kind == OperationKind::TurnOffTheStartupEntry || step.kind == OperationKind::TurnOnTheStartupEntry)
    {
        return RunTheStartupStep(step);
    }

    const LinkOutcome outcome = CreatesALink(step.kind)
        ? linking_.Enable(Addon{.folderPath = step.addonFolder, .manifest = Manifest{}}, step.linkPath.parent_path(),
                          linkType_)
        : linking_.Disable(step.linkPath);

    log_.RecordLink(step.kind, step.addonId, step.addonFolder, step.linkPath, outcome.Failure());

    return LinkOperationResult{.addonId = step.addonId,
                               .addonFolder = step.addonFolder,
                               .linkPath = step.linkPath,
                               .kind = step.kind,
                               .outcome = outcome};
}

LinkBatchReport
ProfileService::SetEnabled(const SimulatorProfile& profile, const ProfileSnapshot& shown, const LinkBatch& batch)
{
    return SetEnabled(profile, shown, batch, ReadLinksNow(profile));
}

LinkBatchReport ProfileService::SetEnabled(const SimulatorProfile& profile,
                                           const ProfileSnapshot& shown,
                                           const LinkBatch& batch,
                                           const LinksOnDisk& onDisk)
{
    std::vector<const TreeNode*> touched = batch.toDisable;
    touched.insert(touched.end(), batch.toEnable.begin(), batch.toEnable.end());
    const std::size_t drifted = AddonsThatDrifted(touched, shown.enabled, onDisk.enabled);

    std::vector<Step> steps = PlanSteps(profile, onDisk, batch.toDisable, false, batch.startupEntriesToTurnOff);
    const std::vector<Step> enabling = PlanSteps(profile, onDisk, batch.toEnable, true);
    steps.insert(steps.end(), enabling.begin(), enabling.end());

    for (const StartupSwitch& request : batch.startupSwitches)
    {
        steps.push_back(
            {.kind = request.enable ? OperationKind::TurnOnTheStartupEntry : OperationKind::TurnOffTheStartupEntry,
             .addonId = IdentityOf(profile, request.line.addonFolder),
             .addonFolder = request.line.addonFolder,
             .linkPath = request.line.path,
             .label = request.line.label});
    }

    return {.results = RunAsOneBatch(steps), .drifted = drifted};
}

std::vector<LinkOperationResult> ProfileService::RunAsOneBatch(const std::vector<Step>& steps)
{
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
        std::ranges::reverse(undo);
        undo_ = std::move(undo);
    }

    return results;
}

LinkBatchReport ProfileService::Relink(const SimulatorProfile& profile,
                                       const ProfileSnapshot& shown,
                                       const std::vector<const TreeNode*>& nodes)
{
    const LinksOnDisk onDisk = ReadLinksNow(profile);

    const std::size_t drifted = AddonsThatDrifted(nodes, shown.enabled, onDisk.enabled);

    const std::vector<Step> unlinking = PlanSteps(profile, onDisk, nodes, false);

    std::vector<Step> steps = unlinking;
    std::set<std::string> relinked;

    for (const Step& step : unlinking)
    {
        if (relinked.insert(ComparablePath(step.addonFolder)).second)
        {
            steps.push_back({.kind = OperationKind::EnableAddon,
                             .addonId = step.addonId,
                             .addonFolder = step.addonFolder,
                             .linkPath = PlannedLinkPath(profile, step.addonFolder)});
        }
    }

    return {.results = RunAsOneBatch(steps), .drifted = drifted};
}

LinkBatchReport ProfileService::SetEnabled(const SimulatorProfile& profile,
                                           const ProfileSnapshot& shown,
                                           const std::vector<const TreeNode*>& nodes,
                                           const bool enable)
{
    return enable ? SetEnabled(profile, shown, LinkBatch{.toDisable = {}, .toEnable = nodes})
                  : SetEnabled(profile, shown, LinkBatch{.toDisable = nodes, .toEnable = {}});
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

        return Step{.kind = OperationKind::RepointLink,
                    .addonId = IdentityOf(profile, *request.candidate.repointTo),
                    .addonFolder = *request.candidate.repointTo,
                    .linkPath = entry.path};
    }

    return Step{.kind = OperationKind::RemoveBrokenLink,
                .addonId = IdentityOf(profile, entry.target),
                .addonFolder = entry.target,
                .linkPath = entry.path};
}

std::vector<ProfileService::Step> ProfileService::Inverse(const SimulatorProfile& profile, const RepairRequest& request)
{
    const DestinationEntry& entry = request.candidate.entry;

    std::vector<Step> undo;

    if (request.action == RepairAction::Repoint)
    {
        undo.push_back({.kind = OperationKind::DisableAddon,
                        .addonId = IdentityOf(profile, *request.candidate.repointTo),
                        .addonFolder = *request.candidate.repointTo,
                        .linkPath = entry.path});
    }

    undo.push_back({.kind = OperationKind::EnableAddon,
                    .addonId = IdentityOf(profile, entry.target),
                    .addonFolder = entry.target,
                    .linkPath = entry.path});

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
