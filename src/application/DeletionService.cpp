#include "application/DeletionService.h"

#include <algorithm>
#include <iterator>

#include "domain/importing/ExternalSidecar.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    OperationKind KindOf(const DeletionRoute route)
    {
        return route == DeletionRoute::RecycleBin ? OperationKind::RecycleFromLibrary
                                                  : OperationKind::DeleteFromLibrary;
    }

    std::vector<std::filesystem::path> LibraryRootsOf(const SimulatorProfile& profile)
    {
        std::vector<std::filesystem::path> roots;
        roots.reserve(profile.libraries.size());

        for (const Library& library : profile.libraries)
        {
            roots.push_back(library.path);
        }

        return roots;
    }
}

DeletionService::DeletionService(const FilesystemProbe& filesystemProbe,
                                 FileOperations& files,
                                 const LinkingEngine& linking,
                                 const EntryClassifier& classifier,
                                 const ProcessProbe& processProbe,
                                 const OperationLog& log,
                                 const SizeService& sizes)
    : filesystemProbe_(filesystemProbe),
      files_(files),
      linking_(linking),
      classifier_(classifier),
      processProbe_(processProbe),
      log_(log),
      sizes_(sizes)
{
}

std::vector<DeletionService::LinksNow>
DeletionService::ReadLinksNow(const std::vector<SimulatorProfile>& everyProfile) const
{
    std::vector<LinksNow> seen;
    seen.reserve(everyProfile.size());

    for (const SimulatorProfile& profile : everyProfile)
    {
        seen.push_back(LinksNow{.profileId = profile.id,
                                .entries = classifier_.Resolve(profile.destinations, LibraryRootsOf(profile))});
    }

    return seen;
}

std::vector<EnabledSomewhere> DeletionService::WhereItIsEnabled(const std::vector<LinksNow>& seen,
                                                                const std::filesystem::path& folder)
{
    std::vector<EnabledSomewhere> enabled;

    for (const LinksNow& profile : seen)
    {
        for (const std::filesystem::path& link : LinksPointingAt(profile.entries, folder))
        {
            enabled.push_back(EnabledSomewhere{.profileId = profile.profileId, .linkPath = link});
        }
    }

    return enabled;
}

std::vector<VolumeRoom> DeletionService::RoomOnEachVolume(const std::vector<AddonToDelete>& addons) const
{
    std::vector<VolumeRoom> volumes;

    for (const AddonToDelete& addon : addons)
    {
        const std::filesystem::path volume = VolumeOf(addon.folder);
        const std::string key = ComparablePath(volume);

        auto found = std::ranges::find_if(volumes,
                                          [&key](const VolumeRoom& room)
                                          {
                                              return ComparablePath(room.volume) == key;
                                          });

        if (found == volumes.end())
        {
            VolumeRoom room{.volume = volume, .selected = std::uintmax_t{0}};

            if (const std::optional<RecycleBinRoom> bin = filesystemProbe_.RecycleBinOn(volume))
            {
                room.quota = bin->quota;
                room.itRecycles = bin->itRecycles;
            }

            volumes.push_back(std::move(room));
            found = std::prev(volumes.end());
        }

        if (!addon.bytes.has_value())
        {
            found->selected.reset();
            continue;
        }

        if (found->selected.has_value())
        {
            *found->selected += *addon.bytes;
        }
    }

    return volumes;
}

DeletionPlan DeletionService::Plan(const SimulatorProfile& profile,
                                   const std::vector<SimulatorProfile>& everyProfile,
                                   const std::vector<const TreeNode*>& nodes) const
{
    const std::vector<LinksNow> seen = ReadLinksNow(everyProfile);

    DeletionPlan plan;

    for (const TreeNode* node : nodes)
    {
        if (node == nullptr || node->kind != TreeNodeKind::Addon)
        {
            ++plan.nodesThatAreNotAddons;
            continue;
        }

        plan.addons.push_back(AddonToDelete{.folder = node->path,
                                            .addonId = IdentityOf(profile, node->path),
                                            .enabled = WhereItIsEnabled(seen, node->path),
                                            .bytes = sizes_.BytesOf(node->path),
                                            .longestEntry = sizes_.LongestEntryOf(node->path)});
    }

    plan.volumes = RoomOnEachVolume(plan.addons);

    return plan;
}

FileResult
DeletionService::TheRouteRefuses(const DeletionPlan& plan, const AddonToDelete& addon, const DeletionRoute route)
{
    return route == DeletionRoute::Permanently ? FileResult::Completed : WhatTheRecycleBinRefuses(plan, addon);
}

DeletionResult DeletionService::DeleteOne(const AddonToDelete& addon,
                                          const std::vector<LinksNow>& seen,
                                          const DeletionRoute route) const
{
    DeletionResult result{.folder = addon.folder};

    for (const EnabledSomewhere& link : WhereItIsEnabled(seen, addon.folder))
    {
        const LinkOutcome outcome = linking_.Disable(link.linkPath);

        log_.RecordLink(OperationKind::DisableAddon, addon.addonId, addon.folder, link.linkPath, outcome.Failure());

        if (!outcome.Succeeded())
        {
            result.result = FileResult::CouldNotRemoveTheLink;

            return result;
        }

        result.linksRemoved.push_back(link.linkPath);
    }

    const bool gone =
        route == DeletionRoute::RecycleBin ? files_.Recycle(addon.folder) : files_.RemoveTree(addon.folder);

    if (gone)
    {
        static_cast<void>(files_.RemoveTree(ExternalSidecarPathFor(addon.folder)));
    }

    result.result = gone ? FileResult::Completed : FileResult::CouldNotDelete;

    log_.RecordImport(KindOf(route), addon.addonId, addon.folder, {}, result.result);

    return result;
}

std::vector<DeletionResult> DeletionService::Delete(const std::vector<SimulatorProfile>& everyProfile,
                                                    const DeletionPlan& plan,
                                                    const DeletionRoute route) const
{
    std::vector<DeletionResult> results;
    results.reserve(plan.addons.size());

    if (plan.addons.empty())
    {
        return results;
    }

    if (processProbe_.SimulatorIsRunning())
    {
        for (const AddonToDelete& addon : plan.addons)
        {
            results.push_back(DeletionResult{.folder = addon.folder, .result = FileResult::TheSimulatorIsRunning});
        }

        return results;
    }

    const std::vector<LinksNow> seen = ReadLinksNow(everyProfile);

    for (const AddonToDelete& addon : plan.addons)
    {
        const FileResult refused = TheRouteRefuses(plan, addon, route);

        if (!Succeeded(refused))
        {
            results.push_back(DeletionResult{.folder = addon.folder, .result = refused});
            continue;
        }

        results.push_back(DeleteOne(addon, seen, route));
    }

    return results;
}
