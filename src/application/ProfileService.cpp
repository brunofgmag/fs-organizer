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

std::vector<StartupEntry> ProfileService::StartupEntriesNow() const
{
    return startup_.Entries();
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

ProfileSnapshot ProfileService::Scan(const SimulatorProfile& profile, const ScanGate& gate) const
{
    ProfileSnapshot snapshot;

    snapshot.libraries = LibraryTreesOf(catalog_, profile, gate);

    if (!gate.StillWanted())
    {
        snapshot.complete = false;

        return snapshot;
    }

    EntriesRead read = Derived({}, ResolveEntries(profile, snapshot.libraries), snapshot.libraries);

    snapshot.entries = std::move(read.entries);
    snapshot.enabled = std::move(read.enabled);
    snapshot.conflicts = std::move(read.conflicts);
    snapshot.startupEntries = std::move(read.startupEntries);

    return snapshot;
}

EntriesRead ProfileService::ReadEntries(const EntriesStamp& stamp, const std::vector<TreeNode>& libraries) const
{
    return Derived(stamp, ResolveEntries(stamp.profile, libraries), libraries);
}

EntriesRead ProfileService::Derived(EntriesStamp stamp,
                                    std::vector<DestinationEntry> entries,
                                    const std::vector<TreeNode>& libraries) const
{
    EntriesRead read{.stamp = std::move(stamp), .entries = std::move(entries)};

    read.enabled = EnabledAddons(EnabledAddonFolders(read.entries));
    read.conflicts = FindCopyConflicts(read.entries, libraries);
    read.startupEntries = startup_.Entries();

    return read;
}

LinkBatchOutcome ProfileService::AfterTheBatch(const EntriesStamp& stamp,
                                               const std::vector<TreeNode>& libraries,
                                               const std::vector<DestinationEntry>& before,
                                               LinkBatchReport report,
                                               const std::vector<ExternalAddon>& externals) const
{
    std::vector<DestinationEntry> after = EntriesAfter(stamp.profile, before, report.results, externals);

    return {.report = std::move(report), .read = Derived(stamp, std::move(after), libraries)};
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

ProfileService::LinksOnDisk ProfileService::ReadLinksNow(const SimulatorProfile& profile,
                                                         const std::vector<TreeNode>& libraries) const
{
    return ReadLinksNow(profile, WhatCameFromAnotherProgram(profile, libraries));
}

ProfileService::LinksOnDisk ProfileService::ReadLinksNow(const SimulatorProfile& profile,
                                                         const std::vector<ExternalAddon>& externals) const
{
    std::vector<DestinationEntry> entries = classifier_.Resolve(profile.destinations, LibraryRoots(profile), externals);
    EnabledAddons enabled{EnabledAddonFolders(entries)};

    return {.entries = std::move(entries), .enabled = std::move(enabled)};
}

std::vector<DestinationEntry> ProfileService::EntriesAfter(const SimulatorProfile& profile,
                                                           const std::vector<DestinationEntry>& before,
                                                           const std::vector<LinkOperationResult>& results,
                                                           const std::vector<ExternalAddon>& externals) const
{
    std::vector<std::filesystem::path> changed;
    changed.reserve(results.size());

    for (const LinkOperationResult& result : results)
    {
        changed.push_back(result.linkPath);
    }

    return classifier_.Refresh(before, changed, profile.destinations, LibraryRoots(profile), externals);
}

std::vector<TakenPlace> ProfileService::PlacesTakenNow(const SimulatorProfile& profile,
                                                       const std::vector<const TreeNode*>& nodes,
                                                       const ProfileSnapshot& shown) const
{
    std::vector<const TreeNode*> wanting;
    std::vector<std::filesystem::path> places;
    std::set<std::string> asked;

    for (const TreeNode* node : nodes)
    {
        for (const TreeNode* addon : AddonsUnder(*node))
        {
            if (shown.enabled.Contains(addon->path) || !asked.insert(ComparablePath(addon->path)).second)
            {
                continue;
            }

            wanting.push_back(addon);
            places.push_back(PlannedLinkPath(profile, addon->path));
        }
    }

    if (classifier_.LinksAt(places, LibraryRoots(profile)).empty())
    {
        return {};
    }

    const std::vector<DestinationEntry> links =
        classifier_.LinksAt(places, LibraryRoots(profile), WhatCameFromAnotherProgram(profile, shown.libraries));
    const std::map<std::string, const DestinationEntry*> held = LinksHeldByPath(links);

    std::vector<TakenPlace> taken;

    for (std::size_t index = 0; index < wanting.size(); ++index)
    {
        const auto occupied = held.find(ComparablePath(places[index]));

        if (occupied != held.end() && ComparablePath(occupied->second->target) != ComparablePath(wanting[index]->path))
        {
            taken.push_back(TakenPlace{
                .addonFolder = wanting[index]->path, .linkPath = places[index], .occupant = occupied->second->target});
        }
    }

    return taken;
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
                                                    const std::vector<const TreeNode*>& nodes,
                                                    const std::vector<TreeNode>& libraries) const
{
    return PlacesTaken(profile, nodes, ReadLinksNow(profile, libraries));
}

std::vector<TakenPlace> ProfileService::PlacesTaken(const SimulatorProfile& profile,
                                                    const std::vector<const TreeNode*>& nodes,
                                                    const LinksOnDisk& onDisk) const
{
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

LinkOperationResult ProfileService::RunTheStartupStep(const Step& step, StartupBackup& backup) const
{
    const bool turningOn = step.kind == OperationKind::TurnOnTheStartupEntry;
    const FileResult result = startup_.Switch(step.linkPath, turningOn, backup);

    log_.RecordImport(step.kind, step.addonId, step.addonFolder, step.linkPath, result, OriginSource::Unknown,
                      step.label);

    return LinkOperationResult{.addonId = step.addonId,
                               .addonFolder = step.addonFolder,
                               .linkPath = step.linkPath,
                               .kind = step.kind,
                               .outcome = LinkOutcome::OfFile(result)};
}

LinkOperationResult ProfileService::Run(const Step& step, StartupBackup& backup) const
{
    if (step.kind == OperationKind::TurnOffTheStartupEntry || step.kind == OperationKind::TurnOnTheStartupEntry)
    {
        return RunTheStartupStep(step, backup);
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
    return SetEnabled(profile, shown, batch, ReadLinksNow(profile, shown.libraries));
}

LinkBatchOutcome
ProfileService::SetEnabled(const EntriesStamp& stamp, const ProfileSnapshot& shown, const LinkBatch& batch)
{
    const std::vector<ExternalAddon> externals = WhatCameFromAnotherProgram(stamp.profile, shown.libraries);
    const LinksOnDisk onDisk = ReadLinksNow(stamp.profile, externals);

    LinkBatchReport report = SetEnabled(stamp.profile, shown, batch, onDisk);

    return AfterTheBatch(stamp, shown.libraries, onDisk.entries, std::move(report), externals);
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
    const std::lock_guard lock(guard_);

    StartupBackup backup;
    std::vector<LinkOperationResult> results;
    std::vector<Step> undo;

    for (const Step& step : steps)
    {
        LinkOperationResult result = Run(step, backup);

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

LinkBatchOutcome ProfileService::Relink(const EntriesStamp& stamp,
                                        const ProfileSnapshot& shown,
                                        const std::vector<const TreeNode*>& nodes)
{
    const SimulatorProfile& profile = stamp.profile;
    const std::vector<ExternalAddon> externals = WhatCameFromAnotherProgram(profile, shown.libraries);
    const LinksOnDisk onDisk = ReadLinksNow(profile, externals);

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

    LinkBatchReport report{.results = RunAsOneBatch(steps), .drifted = drifted};

    return AfterTheBatch(stamp, shown.libraries, onDisk.entries, std::move(report), externals);
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
    const std::lock_guard lock(guard_);

    StartupBackup backup;
    std::vector<LinkOperationResult> results;
    std::vector<Step> undo;

    for (const RepairRequest& request : requests)
    {
        const std::optional<Step> step = PlanRepair(profile, request);
        if (!step.has_value())
        {
            continue;
        }

        LinkOperationResult result = Run(*step, backup);

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
    const std::lock_guard lock(guard_);

    return !undo_.empty();
}

void ProfileService::ForgetUndo()
{
    const std::lock_guard lock(guard_);

    undo_.clear();
}

std::vector<LinkOperationResult> ProfileService::UndoLastBatch()
{
    const std::lock_guard lock(guard_);

    return RunTheUndo();
}

LinkBatchOutcome ProfileService::UndoLastBatch(const EntriesStamp& stamp, const std::vector<TreeNode>& libraries)
{
    const std::lock_guard lock(guard_);

    const std::vector<ExternalAddon> externals = WhatCameFromAnotherProgram(stamp.profile, libraries);
    const LinksOnDisk onDisk = ReadLinksNow(stamp.profile, externals);

    LinkBatchReport report{.results = RunTheUndo()};

    return AfterTheBatch(stamp, libraries, onDisk.entries, std::move(report), externals);
}

std::vector<LinkOperationResult> ProfileService::RunTheUndo()
{
    const std::vector<Step> steps = std::exchange(undo_, {});

    StartupBackup backup;
    std::vector<LinkOperationResult> results;
    for (const Step& step : steps)
    {
        results.push_back(Run(step, backup));
    }

    return results;
}
