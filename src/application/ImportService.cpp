#include "application/ImportService.h"

#include <algorithm>
#include <numeric>
#include <ranges>
#include <set>
#include <string>

#include "domain/importing/ImportPaths.h"
#include "domain/linking/DisableLinks.h"
#include "domain/model/LandingPath.h"
#include "domain/model/Manifest.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/LibraryLookup.h"
#include "domain/tree/LibraryTrees.h"

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
            return {.loser = conflict.destinationPath,
                    .quarantine = QuarantineBesideTheDestination(profile, conflict.destinationPath),
                    .kind = OperationKind::QuarantineFromDestination,
                    .relinks = true};
        }

        return {.loser = conflict.libraryPath,
                .quarantine = QuarantineInsideTheLibrary(profile, conflict.libraryPath),
                .kind = OperationKind::QuarantineFromLibrary,
                .relinks = false};
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
            if (std::ranges::find(kinds, record.kind) != kinds.end() && Succeeded(record.outcome)
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
                             const OperationLog& log,
                             const LinkType linkType)
    : engine_(engine),
      processProbe_(processProbe),
      filesystemProbe_(filesystemProbe),
      catalog_(catalog),
      files_(files),
      linking_(linking),
      log_(log),
      linkType_(linkType)
{
}

void ImportService::UseLinkType(const LinkType linkType)
{
    linkType_ = linkType;
}

std::vector<ImportOperationResult> ImportService::Import(const SimulatorProfile& profile,
                                                         const std::vector<ImportRequest>& requests,
                                                         const std::function<bool(const CopyProgress&)>& onProgress,
                                                         const std::function<void(OperationKind)>& onStep) const
{
    std::vector<ImportOperationResult> results;
    results.reserve(requests.size());

    if (processProbe_.SimulatorIsRunning())
    {
        for (const ImportRequest& request : requests)
        {
            results.push_back(ImportOperationResult{.request = request, .result = FileResult::TheSimulatorIsRunning});
        }

        return results;
    }

    const std::vector<TreeNode> libraries = LibraryTreesOf(catalog_, profile);

    for (const ImportRequest& request : requests)
    {
        if (const TreeNode* occupant = AddonHoldingTheIdentity(libraries, request.Target(), {}))
        {
            results.push_back(ImportOperationResult{
                .request = request, .result = FileResult::TheIdentityIsTaken, .occupant = occupant->path});
            continue;
        }

        results.push_back(ImportOperationResult{
            .request = request, .result = engine_.Import(profile, request, onProgress, onStep).Result()});
    }

    return results;
}

FileResult ImportService::ResolveConflict(const SimulatorProfile& profile,
                                          const std::vector<DestinationEntry>& entries,
                                          const CopyConflict& conflict,
                                          const ConflictChoice choice) const
{
    if (processProbe_.SimulatorIsRunning())
    {
        return FileResult::TheSimulatorIsRunning;
    }

    const Resolution resolution = ResolutionFor(profile, conflict, choice);

    if (resolution.quarantine.empty())
    {
        return FileResult::CouldNotQuarantine;
    }

    const AddonId addon = IdentityOf(profile, conflict.libraryPath);

    if (!DisableEveryLink(linking_, log_, LinksPointingAt(entries, resolution.loser), addon, resolution.loser))
    {
        return FileResult::CouldNotRemoveTheLink;
    }

    const bool moved = files_.Move(resolution.loser, resolution.quarantine);

    log_.RecordImport(resolution.kind, addon, resolution.loser, resolution.quarantine,
                      moved ? FileResult::Completed : FileResult::CouldNotQuarantine);

    if (!moved)
    {
        return FileResult::CouldNotQuarantine;
    }

    if (!resolution.relinks)
    {
        return FileResult::Completed;
    }

    const LinkOutcome link =
        linking_.Enable(Addon{.folderPath = conflict.libraryPath}, conflict.destinationPath.parent_path(), linkType_);

    log_.RecordLink(OperationKind::EnableAddon, addon, conflict.libraryPath, conflict.destinationPath, link.Failure());

    return link.Succeeded() ? FileResult::Completed : FileResult::CouldNotCreateLink;
}

ConflictSide ImportService::SideOf(const std::filesystem::path& folder) const
{
    const std::vector<FileFingerprint> files =
        filesystemProbe_.FingerprintTree(folder).value_or(std::vector<FileFingerprint>{});
    const auto sizes = files | std::views::transform(&FileFingerprint::size);

    const TreeNode scanned = catalog_.Scan(folder);

    return ConflictSide{.path = folder,
                        .manifest = scanned.addon.has_value() ? scanned.addon->manifest : Manifest{},
                        .sizeBytes = std::accumulate(sizes.begin(), sizes.end(), std::uintmax_t{0}),
                        .modified = filesystemProbe_.LastWriteTime(folder)};
}

ConflictDetails ImportService::DetailsOf(const std::vector<DestinationEntry>& entries,
                                         const CopyConflict& conflict) const
{
    return ConflictDetails{.destination = SideOf(conflict.destinationPath),
                           .library = SideOf(conflict.libraryPath),
                           .linksToTheLibraryCopy = LinksPointingAt(entries, conflict.libraryPath)};
}

std::uintmax_t ImportService::TotalSizeOf(const std::vector<std::filesystem::path>& folders) const
{
    std::uintmax_t total = 0;

    for (const std::filesystem::path& folder : folders)
    {
        const std::vector<FileFingerprint> files =
            filesystemProbe_.FingerprintTree(folder).value_or(std::vector<FileFingerprint>{});
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
                           const FileResult result) const
{
    log_.RecordImport(kind, IdentityOf(profile, addonFolder), source, target, result);
}

std::vector<QuarantinedItem> ImportService::Quarantined(const SimulatorProfile& profile) const
{
    const std::vector<OperationRecord> history = log_.History();

    std::vector<QuarantinedItem> items;

    for (const std::filesystem::path& folder : QuarantineFoldersOf(profile))
    {
        for (const std::filesystem::path& item : filesystemProbe_.ChildDirectories(folder))
        {
            const OperationRecord* quarantined = LastRecordAbout(
                history, item, {OperationKind::QuarantineFromDestination, OperationKind::QuarantineFromLibrary});

            items.push_back(QuarantinedItem{
                .path = item,
                .origin = quarantined == nullptr ? std::filesystem::path{} : quarantined->source,
                .quarantinedAt = quarantined == nullptr ? std::nullopt : std::optional(quarantined->timestamp)});
        }
    }

    return items;
}

std::string ImportService::VersionIn(const std::filesystem::path& folder) const
{
    const TreeNode scanned = catalog_.Scan(folder);

    return scanned.addon.has_value() ? scanned.addon->manifest.packageVersion : std::string{};
}

std::vector<QuarantineDetail> ImportService::Describe(const std::vector<DestinationEntry>& entries,
                                                      const std::vector<QuarantinedItem>& items) const
{
    std::vector<QuarantineDetail> details;
    details.reserve(items.size());

    for (const QuarantinedItem& item : items)
    {
        QuarantineDetail detail{.path = item.path, .version = VersionIn(item.path)};

        const std::string name = ComparableFileName(item.path);
        const auto occupant =
            std::ranges::find_if(entries,
                                 [&name](const DestinationEntry& entry)
                                 {
                                     return entry.target.empty() && ComparableFileName(entry.path) == name;
                                 });

        if (occupant != entries.end())
        {
            detail.replacedBy = occupant->path;
            detail.replacementVersion = VersionIn(occupant->path);
        }

        details.push_back(std::move(detail));
    }

    return details;
}

RestoreCheck ImportService::CheckOne(const std::vector<TreeNode>& libraries, const QuarantinedItem& item) const
{
    RestoreCheck check{.item = item, .target = item.origin};

    if (!item.KnowsWhereItCameFrom())
    {
        check.result = FileResult::TheOriginIsUnknown;

        return check;
    }

    if (const TreeNode* occupant = AddonHoldingTheIdentity(libraries, item.origin, {}))
    {
        check.result = FileResult::TheIdentityIsTaken;
        check.occupant = occupant->path;
    }
    else if (filesystemProbe_.EntryExistsWithoutFollowingLinks(item.origin))
    {
        check.result = FileResult::TheOriginIsOccupied;
        check.occupant = item.origin;
    }

    if (check.CanProceed())
    {
        return check;
    }

    check.version = VersionIn(item.path);
    check.occupantVersion = VersionIn(check.occupant);

    return check;
}

std::vector<RestoreCheck> ImportService::CheckRestore(const SimulatorProfile& profile,
                                                      const std::vector<QuarantinedItem>& items) const
{
    const std::vector<TreeNode> libraries = LibraryTreesOf(catalog_, profile);

    std::vector<RestoreCheck> checks;
    checks.reserve(items.size());

    for (const QuarantinedItem& item : items)
    {
        checks.push_back(CheckOne(libraries, item));
    }

    return checks;
}

std::vector<RestorePlace> ImportService::PlacesFor(const SimulatorProfile& profile, const QuarantinedItem& item) const
{
    const std::string folder = ComparablePath(item.path.parent_path());

    std::vector<RestorePlace> places;

    for (const std::filesystem::path& destination : profile.destinations)
    {
        if (ComparablePath(QuarantineFolderBeside(destination)) == folder)
        {
            places.push_back(RestorePlace{
                .place = destination, .target = LandingPathIn(destination, item.path), .label = destination});
        }
    }

    for (const Library& library : profile.libraries)
    {
        if (ComparablePath(QuarantineFolderInside(library.path)) != folder)
        {
            continue;
        }

        const TreeNode tree = catalog_.Scan(library.path);

        for (const TreeNode* category : CategoriesOfferedIn(tree, true))
        {
            places.push_back(RestorePlace{.place = category->path,
                                          .target = LandingPathIn(category->path, item.path),
                                          .label = category->path.lexically_relative(library.path)});
        }
    }

    return places;
}

FileResult ImportService::RestoreOne(const SimulatorProfile& profile, const QuarantinedItem& item) const
{
    const bool moved = files_.Move(item.path, item.origin);
    const FileResult result = moved ? FileResult::Completed : FileResult::CouldNotRestore;

    Record(profile, OperationKind::RestoreFromQuarantine, item.origin, item.path, item.origin, result);

    return result;
}

FileResult ImportService::DiscardOne(const SimulatorProfile& profile, const QuarantinedItem& item) const
{
    if (!IsQuarantineFolder(item.path.parent_path()))
    {
        return FileResult::CouldNotDiscard;
    }

    const bool removed = files_.RemoveTree(item.path);
    const FileResult result = removed ? FileResult::Completed : FileResult::CouldNotDiscard;

    Record(profile, OperationKind::DiscardFromQuarantine, item.origin.empty() ? item.path : item.origin, item.path, {},
           result);

    return result;
}

std::vector<FileOperationResult> ImportService::Restore(const SimulatorProfile& profile,
                                                        const std::vector<QuarantinedItem>& items) const
{
    std::vector<FileOperationResult> results;
    results.reserve(items.size());

    if (processProbe_.SimulatorIsRunning())
    {
        for (const QuarantinedItem& item : items)
        {
            results.push_back(FileOperationResult{.path = item.path, .result = FileResult::TheSimulatorIsRunning});
        }

        return results;
    }

    for (const RestoreCheck& check : CheckRestore(profile, items))
    {
        if (!check.CanProceed())
        {
            results.push_back(
                FileOperationResult{.path = check.item.path, .result = check.result, .occupant = check.occupant});
            continue;
        }

        results.push_back(FileOperationResult{.path = check.item.path, .result = RestoreOne(profile, check.item)});
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
        results.push_back(FileOperationResult{
            .path = item.path, .result = blocked ? FileResult::TheSimulatorIsRunning : DiscardOne(profile, item)});
    }

    return results;
}

std::vector<StagingLeftover> ImportService::Leftovers(const SimulatorProfile& profile) const
{
    const std::vector<OperationRecord> history = log_.History();

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

            leftovers.push_back(
                StagingLeftover{.staging = child,
                                .target = ImportedPathFor(child),
                                .source = copied == nullptr ? std::filesystem::path{} : copied->source});
        }
    }

    return leftovers;
}

FileResult ImportService::DiscardOneStaging(const SimulatorProfile& profile, const StagingLeftover& leftover) const
{
    if (!IsStagingPath(leftover.staging))
    {
        return FileResult::CouldNotDiscard;
    }

    const bool removed = files_.RemoveTree(leftover.staging);
    const FileResult result = removed ? FileResult::Completed : FileResult::CouldNotDiscard;

    Record(profile, OperationKind::DiscardStaging, leftover.target, leftover.staging, {}, result);

    return result;
}

std::vector<ImportOperationResult> ImportService::Resume(const SimulatorProfile& profile,
                                                         const std::vector<StagingLeftover>& leftovers,
                                                         const std::function<bool(const CopyProgress&)>& onProgress,
                                                         const std::function<void(OperationKind)>& onStep) const
{
    const bool blocked = processProbe_.SimulatorIsRunning();
    const std::vector<TreeNode> libraries = blocked ? std::vector<TreeNode>{} : LibraryTreesOf(catalog_, profile);

    std::vector<ImportOperationResult> results;
    results.reserve(leftovers.size());

    for (const StagingLeftover& leftover : leftovers)
    {
        const ImportRequest request{.source = leftover.source, .category = leftover.target.parent_path()};

        if (blocked)
        {
            results.push_back(ImportOperationResult{.request = request, .result = FileResult::TheSimulatorIsRunning});
            continue;
        }

        if (const TreeNode* occupant = AddonHoldingTheIdentity(libraries, leftover.target, {}))
        {
            results.push_back(ImportOperationResult{
                .request = request, .result = FileResult::TheIdentityIsTaken, .occupant = occupant->path});
            continue;
        }

        if (!leftover.CanBeResumed())
        {
            results.push_back(ImportOperationResult{.request = request, .result = FileResult::TheOriginIsUnknown});
            continue;
        }

        if (const FileResult discarded = DiscardOneStaging(profile, leftover); !Succeeded(discarded))
        {
            results.push_back(ImportOperationResult{.request = request, .result = discarded});
            continue;
        }

        results.push_back(ImportOperationResult{
            .request = request, .result = engine_.Import(profile, request, onProgress, onStep).Result()});
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
        results.push_back(FileOperationResult{.path = leftover.staging,
                                              .result = blocked ? FileResult::TheSimulatorIsRunning
                                                                : DiscardOneStaging(profile, leftover)});
    }

    return results;
}
