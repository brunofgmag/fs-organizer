#include "application/ImportService.h"

#include <algorithm>
#include <numeric>
#include <ranges>
#include <set>
#include <string>

#include "domain/importing/ImportPaths.h"
#include "domain/model/Manifest.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    std::filesystem::path QuarantineBesideTheDestination(const SimulatorProfile& profile,
                                                         const std::filesystem::path& entry)
    {
        const auto destination = std::ranges::find_if(profile.destinations,
                                                      [&entry](const std::filesystem::path& candidate)
                                                      {
                                                          return PathIsInside(entry, candidate);
                                                      });

        return destination == profile.destinations.end() ? std::filesystem::path{}
                                                         : QuarantineFolderBeside(*destination) / entry.filename();
    }

    std::filesystem::path QuarantineInsideTheLibrary(const SimulatorProfile& profile,
                                                     const std::filesystem::path& addon)
    {
        const Library* library = LibraryContaining(profile, addon);

        return library == nullptr ? std::filesystem::path{} : QuarantineFolderInside(library->path) / addon.filename();
    }

    struct Resolution
    {
        std::filesystem::path loser;
        std::filesystem::path quarantine;
        OperationKind kind = OperationKind::QuarantineFromDestination;
        bool relinks = false;
    };

    Resolution ResolutionFor(const SimulatorProfile& profile, const CopyConflict& conflict, const ConflictChoice choice)
    {
        if (choice == ConflictChoice::KeepTheLibraryCopy)
        {
            return {conflict.destinationPath, QuarantineBesideTheDestination(profile, conflict.destinationPath),
                    OperationKind::QuarantineFromDestination, true};
        }

        return {conflict.libraryPath, QuarantineInsideTheLibrary(profile, conflict.libraryPath),
                OperationKind::QuarantineFromLibrary, false};
    }

    std::vector<std::filesystem::path> QuarantineFoldersOf(const SimulatorProfile& profile)
    {
        std::vector<std::filesystem::path> folders;
        std::set<std::string> seen;

        const auto remember = [&folders, &seen](const std::filesystem::path& folder)
        {
            if (seen.insert(ComparablePath(folder)).second)
            {
                folders.push_back(folder);
            }
        };

        for (const std::filesystem::path& destination : profile.destinations)
        {
            remember(QuarantineFolderBeside(destination));
        }

        for (const Library& library : profile.libraries)
        {
            remember(QuarantineFolderInside(library.path));
        }

        return folders;
    }

    const OperationRecord* LastRecordAbout(const std::vector<OperationRecord>& history,
                                           const std::filesystem::path& target,
                                           const std::vector<OperationKind>& kinds)
    {
        const std::string wanted = ComparablePath(target);

        for (const OperationRecord& record : std::ranges::reverse_view(history))
        {
            if (std::ranges::find(kinds, record.kind) != kinds.end() && record.importResult == ImportResult::Completed
                && ComparablePath(record.target) == wanted)
            {
                return &record;
            }
        }

        return nullptr;
    }
}

ImportService::ImportService(const ImportEngine& engine,
                             const ProcessProbe& processProbe,
                             const FilesystemProbe& filesystemProbe,
                             const CatalogScanner& catalog,
                             FileOperations& files,
                             const LinkingEngine& linking,
                             OperationJournal& journal,
                             const Clock& clock,
                             const LinkType linkType)
    : engine_(engine),
      processProbe_(processProbe),
      filesystemProbe_(filesystemProbe),
      catalog_(catalog),
      files_(files),
      linking_(linking),
      journal_(journal),
      clock_(clock),
      linkType_(linkType)
{
}

std::vector<ImportOperationResult> ImportService::Import(const SimulatorProfile& profile,
                                                         const std::vector<ImportRequest>& requests,
                                                         const std::function<bool(const CopyProgress&)>& onProgress,
                                                         const std::function<void(OperationKind)>& onStep) const
{
    std::vector<ImportOperationResult> results;
    results.reserve(requests.size());

    const bool blocked = processProbe_.SimulatorIsRunning();

    for (const ImportRequest& request : requests)
    {
        results.push_back(ImportOperationResult{request,
                                                blocked
                                                    ? ImportResult::TheSimulatorIsRunning
                                                    : engine_.Import(profile, request, onProgress, onStep).Result()});
    }

    return results;
}

ImportResult ImportService::ResolveConflict(const SimulatorProfile& profile,
                                            const std::vector<DestinationEntry>& entries,
                                            const CopyConflict& conflict,
                                            const ConflictChoice choice) const
{
    if (processProbe_.SimulatorIsRunning())
    {
        return ImportResult::TheSimulatorIsRunning;
    }

    const Resolution resolution = ResolutionFor(profile, conflict, choice);

    if (resolution.quarantine.empty())
    {
        return ImportResult::CouldNotQuarantine;
    }

    const AddonId addon = IdentityOf(profile, conflict.libraryPath);

    if (!UnlinkWhatPointsAtTheLoser(profile, entries, resolution.loser, addon))
    {
        return ImportResult::CouldNotRemoveTheLink;
    }

    const bool moved = files_.Move(resolution.loser, resolution.quarantine);

    journal_.Append(OperationRecord::OfImport(clock_.Now(), resolution.kind, addon, resolution.loser,
                                              resolution.quarantine,
                                              moved ? ImportResult::Completed : ImportResult::CouldNotQuarantine));

    if (!moved)
    {
        return ImportResult::CouldNotQuarantine;
    }

    if (!resolution.relinks)
    {
        return ImportResult::Completed;
    }

    const LinkOutcome link =
        linking_.Enable(Addon{conflict.libraryPath}, conflict.destinationPath.parent_path(), linkType_);

    journal_.Append(OperationRecord::OfLink(clock_.Now(), OperationKind::EnableAddon, addon, conflict.libraryPath,
                                            conflict.destinationPath, link.Failure()));

    return link.Succeeded() ? ImportResult::Completed : ImportResult::CouldNotCreateLink;
}

bool ImportService::UnlinkWhatPointsAtTheLoser(const SimulatorProfile& profile,
                                               const std::vector<DestinationEntry>& entries,
                                               const std::filesystem::path& loser,
                                               const AddonId& addon) const
{
    for (const std::filesystem::path& link : LinksPointingAt(entries, loser))
    {
        const LinkOutcome outcome = linking_.Disable(link);

        journal_.Append(
            OperationRecord::OfLink(clock_.Now(), OperationKind::DisableAddon, addon, link, loser, outcome.Failure()));

        if (!outcome.Succeeded())
        {
            return false;
        }
    }

    return true;
}

ConflictSide ImportService::SideOf(const std::filesystem::path& folder) const
{
    const std::vector<FileFingerprint> files = filesystemProbe_.FingerprintTree(folder);
    const auto sizes = files | std::views::transform(&FileFingerprint::size);

    const TreeNode scanned = catalog_.Scan(folder);

    return ConflictSide{folder, scanned.addon.has_value() ? scanned.addon->manifest : Manifest{},
                        std::accumulate(sizes.begin(), sizes.end(), std::uintmax_t{0}),
                        filesystemProbe_.LastWriteTime(folder)};
}

ConflictDetails ImportService::DetailsOf(const std::vector<DestinationEntry>& entries,
                                         const CopyConflict& conflict) const
{
    return ConflictDetails{SideOf(conflict.destinationPath), SideOf(conflict.libraryPath),
                           LinksPointingAt(entries, conflict.libraryPath)};
}

std::uintmax_t ImportService::TotalSizeOf(const std::vector<std::filesystem::path>& folders) const
{
    std::uintmax_t total = 0;

    for (const std::filesystem::path& folder : folders)
    {
        const std::vector<FileFingerprint> files = filesystemProbe_.FingerprintTree(folder);
        const auto sizes = files | std::views::transform(&FileFingerprint::size);

        total += std::accumulate(sizes.begin(), sizes.end(), std::uintmax_t{0});
    }

    return total;
}

void ImportService::Record(const SimulatorProfile& profile,
                           const OperationKind kind,
                           const std::filesystem::path& addonFolder,
                           const std::filesystem::path& source,
                           const std::filesystem::path& target,
                           const ImportResult result) const
{
    journal_.Append(
        OperationRecord::OfImport(clock_.Now(), kind, IdentityOf(profile, addonFolder), source, target, result));
}

std::vector<QuarantinedItem> ImportService::Quarantined(const SimulatorProfile& profile) const
{
    const std::vector<OperationRecord> history = journal_.Read();

    std::vector<QuarantinedItem> items;

    for (const std::filesystem::path& folder : QuarantineFoldersOf(profile))
    {
        for (const std::filesystem::path& item : filesystemProbe_.ChildDirectories(folder))
        {
            const OperationRecord* quarantined = LastRecordAbout(
                history, item, {OperationKind::QuarantineFromDestination, OperationKind::QuarantineFromLibrary});

            items.push_back(
                QuarantinedItem{item, quarantined == nullptr ? std::filesystem::path{} : quarantined->source,
                                quarantined == nullptr ? std::nullopt : std::optional(quarantined->timestamp)});
        }
    }

    return items;
}

ImportResult ImportService::RestoreOne(const SimulatorProfile& profile, const QuarantinedItem& item) const
{
    if (!item.KnowsWhereItCameFrom())
    {
        return ImportResult::TheOriginIsUnknown;
    }

    if (filesystemProbe_.EntryExistsWithoutFollowingLinks(item.origin))
    {
        return ImportResult::CouldNotRestore;
    }

    const bool moved = files_.Move(item.path, item.origin);
    const ImportResult result = moved ? ImportResult::Completed : ImportResult::CouldNotRestore;

    Record(profile, OperationKind::RestoreFromQuarantine, item.origin, item.path, item.origin, result);

    return result;
}

ImportResult ImportService::DiscardOne(const SimulatorProfile& profile, const QuarantinedItem& item) const
{
    if (!IsQuarantineFolder(item.path.parent_path()))
    {
        return ImportResult::CouldNotDiscard;
    }

    const bool removed = files_.RemoveTree(item.path);
    const ImportResult result = removed ? ImportResult::Completed : ImportResult::CouldNotDiscard;

    Record(profile, OperationKind::DiscardFromQuarantine, item.origin.empty() ? item.path : item.origin, item.path, {},
           result);

    return result;
}

std::vector<FileOperationResult> ImportService::Restore(const SimulatorProfile& profile,
                                                        const std::vector<QuarantinedItem>& items) const
{
    const bool blocked = processProbe_.SimulatorIsRunning();

    std::vector<FileOperationResult> results;
    results.reserve(items.size());

    for (const QuarantinedItem& item : items)
    {
        results.push_back(
            FileOperationResult{item.path, blocked ? ImportResult::TheSimulatorIsRunning : RestoreOne(profile, item)});
    }

    return results;
}

std::vector<FileOperationResult> ImportService::Discard(const SimulatorProfile& profile,
                                                        const std::vector<QuarantinedItem>& items) const
{
    const bool blocked = processProbe_.SimulatorIsRunning();

    std::vector<FileOperationResult> results;
    results.reserve(items.size());

    for (const QuarantinedItem& item : items)
    {
        results.push_back(
            FileOperationResult{item.path, blocked ? ImportResult::TheSimulatorIsRunning : DiscardOne(profile, item)});
    }

    return results;
}

std::vector<StagingLeftover> ImportService::Leftovers(const SimulatorProfile& profile) const
{
    const std::vector<OperationRecord> history = journal_.Read();

    std::vector<StagingLeftover> leftovers;
    std::vector<std::filesystem::path> pending;

    for (const Library& library : profile.libraries)
    {
        pending.push_back(library.path);
    }

    while (!pending.empty())
    {
        const std::filesystem::path folder = pending.back();
        pending.pop_back();

        for (const std::filesystem::path& child : filesystemProbe_.ChildDirectories(folder))
        {
            if (filesystemProbe_.IsReparsePoint(child) || IsQuarantineFolder(child))
            {
                continue;
            }

            if (!IsStagingPath(child))
            {
                if (!filesystemProbe_.EntryExistsWithoutFollowingLinks(ManifestPathIn(child)))
                {
                    pending.push_back(child);
                }

                continue;
            }

            const OperationRecord* copied = LastRecordAbout(history, child, {OperationKind::ImportCopyToStaging});

            leftovers.push_back(StagingLeftover{child, ImportedPathFor(child),
                                                copied == nullptr ? std::filesystem::path{} : copied->source});
        }
    }

    return leftovers;
}

ImportResult ImportService::DiscardOneStaging(const SimulatorProfile& profile, const StagingLeftover& leftover) const
{
    if (!IsStagingPath(leftover.staging))
    {
        return ImportResult::CouldNotDiscard;
    }

    const bool removed = files_.RemoveTree(leftover.staging);
    const ImportResult result = removed ? ImportResult::Completed : ImportResult::CouldNotDiscard;

    Record(profile, OperationKind::DiscardStaging, leftover.target, leftover.staging, {}, result);

    return result;
}

std::vector<ImportOperationResult> ImportService::Resume(const SimulatorProfile& profile,
                                                         const std::vector<StagingLeftover>& leftovers,
                                                         const std::function<bool(const CopyProgress&)>& onProgress,
                                                         const std::function<void(OperationKind)>& onStep) const
{
    const bool blocked = processProbe_.SimulatorIsRunning();

    std::vector<ImportOperationResult> results;
    results.reserve(leftovers.size());

    for (const StagingLeftover& leftover : leftovers)
    {
        const ImportRequest request{leftover.source, leftover.target.parent_path()};

        if (blocked)
        {
            results.push_back(ImportOperationResult{request, ImportResult::TheSimulatorIsRunning});
            continue;
        }

        if (!leftover.CanBeResumed())
        {
            results.push_back(ImportOperationResult{request, ImportResult::TheOriginIsUnknown});
            continue;
        }

        if (const ImportResult discarded = DiscardOneStaging(profile, leftover); discarded != ImportResult::Completed)
        {
            results.push_back(ImportOperationResult{request, discarded});
            continue;
        }

        results.push_back(
            ImportOperationResult{request, engine_.Import(profile, request, onProgress, onStep).Result()});
    }

    return results;
}

std::vector<FileOperationResult> ImportService::DiscardLeftovers(const SimulatorProfile& profile,
                                                                 const std::vector<StagingLeftover>& leftovers) const
{
    const bool blocked = processProbe_.SimulatorIsRunning();

    std::vector<FileOperationResult> results;
    results.reserve(leftovers.size());

    for (const StagingLeftover& leftover : leftovers)
    {
        results.push_back(FileOperationResult{
            leftover.staging, blocked ? ImportResult::TheSimulatorIsRunning : DiscardOneStaging(profile, leftover)});
    }

    return results;
}
