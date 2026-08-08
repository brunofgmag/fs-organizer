#include "application/ImportService.h"

#include <algorithm>
#include <numeric>
#include <ranges>
#include <set>
#include <string>

#include "domain/importing/ImportPaths.h"
#include "domain/importing/OriginSidecar.h"
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
            return {.loser = conflict.provenancePath,
                    .quarantine = QuarantineBesideTheDestination(profile, conflict.provenancePath),
                    .kind = OperationKind::QuarantineFromDestination,
                    .relinks = true};
        }

        return {.loser = conflict.libraryPath,
                .quarantine = QuarantineInsideTheLibrary(profile, conflict.libraryPath),
                .kind = OperationKind::QuarantineFromLibrary,
                .relinks = false};
    }

    Resolution WhereTheOccupantGoes(const SimulatorProfile& profile, const std::filesystem::path& occupant)
    {
        const std::filesystem::path beside = QuarantineBesideTheDestination(profile, occupant);

        if (!beside.empty())
        {
            return {.loser = occupant, .quarantine = beside, .kind = OperationKind::QuarantineFromDestination};
        }

        return {.loser = occupant,
                .quarantine = QuarantineInsideTheLibrary(profile, occupant),
                .kind = OperationKind::QuarantineFromLibrary};
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

    if (const FileResult quarantined = QuarantineInto(resolution.quarantine, resolution.loser, addon, resolution.kind);
        !Succeeded(quarantined))
    {
        return quarantined;
    }

    if (!resolution.relinks)
    {
        return FileResult::Completed;
    }

    const LinkOutcome link =
        linking_.Enable(Addon{.folderPath = conflict.libraryPath}, conflict.provenancePath.parent_path(), linkType_);

    log_.RecordLink(OperationKind::EnableAddon, addon, conflict.libraryPath, conflict.provenancePath, link.Failure());

    return link.Succeeded() ? FileResult::Completed : FileResult::CouldNotCreateLink;
}

ConflictSide ImportService::SideOf(const std::filesystem::path& folder) const
{
    const TreeFingerprint walked = filesystemProbe_.FingerprintTree(folder).value_or(TreeFingerprint{});
    const auto sizes = walked.files | std::views::transform(&FileFingerprint::size);

    const TreeNode scanned = catalog_.Scan(folder);

    return ConflictSide{.path = folder,
                        .manifest = scanned.addon.has_value() ? scanned.addon->manifest : Manifest{},
                        .sizeBytes = std::accumulate(sizes.begin(), sizes.end(), std::uintmax_t{0}),
                        .modified = filesystemProbe_.LastWriteTime(folder)};
}

ConflictDetails ImportService::DetailsOf(const std::vector<DestinationEntry>& entries,
                                         const CopyConflict& conflict) const
{
    return ConflictDetails{.provenance = SideOf(conflict.provenancePath),
                           .library = SideOf(conflict.libraryPath),
                           .linksToTheLibraryCopy = LinksPointingAt(entries, conflict.libraryPath),
                           .theProvenanceIsAnotherProgram = conflict.theProvenanceIsAnotherProgram};
}

std::uintmax_t ImportService::TotalSizeOf(const std::vector<std::filesystem::path>& folders) const
{
    std::uintmax_t total = 0;

    for (const std::filesystem::path& folder : folders)
    {
        const TreeFingerprint walked = filesystemProbe_.FingerprintTree(folder).value_or(TreeFingerprint{});
        const auto sizes = walked.files | std::views::transform(&FileFingerprint::size);

        total += std::accumulate(sizes.begin(), sizes.end(), std::uintmax_t{0});
    }

    return total;
}

FileResult ImportService::QuarantineInto(const std::filesystem::path& quarantine,
                                         const std::filesystem::path& loser,
                                         const AddonId& addon,
                                         const OperationKind kind) const
{
    if (filesystemProbe_.EntryExistsWithoutFollowingLinks(quarantine))
    {
        log_.RecordImport(kind, addon, loser, quarantine, FileResult::CouldNotQuarantine);

        return FileResult::CouldNotQuarantine;
    }

    const std::filesystem::path sidecar = SidecarPathFor(quarantine);

    static_cast<void>(files_.CreateFolder(quarantine.parent_path()));

    if (!files_.WriteTextFile(sidecar, TextOfTheOrigin(QuarantineOrigin{.origin = loser, .quarantinedAt = log_.Now()})))
    {
        log_.RecordImport(kind, addon, loser, quarantine, FileResult::CouldNotRecordTheOrigin);

        return FileResult::CouldNotRecordTheOrigin;
    }

    const bool moved = files_.Move(loser, quarantine);

    if (!moved)
    {
        static_cast<void>(files_.RemoveTree(sidecar));
    }

    log_.RecordImport(kind, addon, loser, quarantine, moved ? FileResult::Completed : FileResult::CouldNotQuarantine);

    return moved ? FileResult::Completed : FileResult::CouldNotQuarantine;
}

void ImportService::ForgetTheOriginOf(const std::filesystem::path& item) const
{
    static_cast<void>(files_.RemoveTree(SidecarPathFor(item)));
}

void ImportService::Record(const SimulatorProfile& profile,
                           const OperationKind kind,
                           const std::filesystem::path& addonFolder,
                           const std::filesystem::path& source,
                           const std::filesystem::path& target,
                           const FileResult result,
                           const OriginSource originSource) const
{
    log_.RecordImport(kind, IdentityOf(profile, addonFolder), source, target, result, originSource);
}

QuarantinedItem ImportService::WhereItCameFrom(const std::vector<OperationRecord>& history,
                                               const std::filesystem::path& item) const
{
    const std::optional<std::string> written = filesystemProbe_.ContentsOf(SidecarPathFor(item));

    const OperationRecord* quarantined = LastRecordAbout(
        history, item, {OperationKind::QuarantineFromDestination, OperationKind::QuarantineFromLibrary});

    if (const std::optional<QuarantineOrigin> beside = written.has_value() ? OriginFromText(*written) : std::nullopt)
    {
        return QuarantinedItem{.path = item,
                               .origin = beside->origin,
                               .quarantinedAt = beside->quarantinedAt,
                               .source = OriginSource::Sidecar,
                               .theOtherSourceSays =
                                   quarantined == nullptr ? std::filesystem::path{} : quarantined->source};
    }

    if (quarantined == nullptr)
    {
        return QuarantinedItem{.path = item};
    }

    return QuarantinedItem{.path = item,
                           .origin = quarantined->source,
                           .quarantinedAt = quarantined->timestamp,
                           .source = OriginSource::Journal};
}

std::vector<QuarantinedItem> ImportService::Quarantined(const SimulatorProfile& profile) const
{
    const std::vector<OperationRecord> history = log_.History();

    std::vector<QuarantinedItem> items;

    for (const std::filesystem::path& folder : QuarantineFoldersOf(profile))
    {
        for (const std::filesystem::path& item : filesystemProbe_.ChildDirectories(folder))
        {
            items.push_back(WhereItCameFrom(history, item));
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

    const TreeNode held = catalog_.Scan(check.occupant);

    check.version = VersionIn(item.path);
    check.occupantIsAnAddon = held.addon.has_value();
    check.occupantVersion = held.addon.has_value() ? held.addon->manifest.packageVersion : std::string{};

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

FileResult ImportService::RestoreOne(const SimulatorProfile& profile,
                                     const QuarantinedItem& item,
                                     const std::filesystem::path& recordedFrom) const
{
    const bool moved = files_.Move(item.path, item.origin);
    const FileResult result = moved ? FileResult::Completed : FileResult::CouldNotRestore;

    if (moved)
    {
        ForgetTheOriginOf(item.path);
    }

    Record(profile, OperationKind::RestoreFromQuarantine, item.origin, recordedFrom.empty() ? item.path : recordedFrom,
           item.origin, result, item.source);

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

    if (removed)
    {
        ForgetTheOriginOf(item.path);
    }

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

SwapResult ImportService::Swap(const SimulatorProfile& profile,
                               const std::vector<DestinationEntry>& entries,
                               const QuarantinedItem& item) const
{
    SwapResult swapped{.item = item.path};

    if (processProbe_.SimulatorIsRunning())
    {
        swapped.result = FileResult::TheSimulatorIsRunning;

        return swapped;
    }

    const RestoreCheck check = CheckOne(LibraryTreesOf(catalog_, profile), item);

    swapped.occupant = check.occupant;
    swapped.inTheLibrary = check.occupant;

    if (!check.CanProceed() && !check.CanBeSwapped())
    {
        swapped.result = check.result;

        return swapped;
    }

    if (check.CanProceed())
    {
        return TheItemComesBack(profile, item, swapped);
    }

    const Resolution goes = WhereTheOccupantGoes(profile, check.occupant);

    if (goes.quarantine.empty())
    {
        swapped.result = FileResult::CouldNotQuarantine;

        return swapped;
    }

    const AddonId occupant = IdentityOf(profile, check.occupant);

    if (!DisableEveryLink(linking_, log_, LinksPointingAt(entries, check.occupant), occupant, check.occupant))
    {
        swapped.result = FileResult::CouldNotRemoveTheLink;

        return swapped;
    }

    QuarantinedItem waiting = item;

    if (ComparablePath(goes.quarantine) == ComparablePath(item.path))
    {
        waiting.path = SwapSlotFor(item.path);

        if (!files_.Move(item.path, waiting.path))
        {
            swapped.result = FileResult::CouldNotQuarantine;

            return swapped;
        }
    }

    if (const FileResult quarantined = QuarantineInto(goes.quarantine, goes.loser, occupant, goes.kind);
        !Succeeded(quarantined))
    {
        if (waiting.path != item.path)
        {
            static_cast<void>(files_.Move(waiting.path, item.path));
        }

        swapped.result = quarantined;

        return swapped;
    }

    return TheItemComesBack(profile, waiting, swapped);
}

SwapResult
ImportService::TheItemComesBack(const SimulatorProfile& profile, const QuarantinedItem& item, SwapResult swapped) const
{
    swapped.stoppedAt = SwapStep::RestoreTheItem;
    swapped.result = RestoreOne(profile, item, swapped.item);
    swapped.inTheLibrary = swapped.Succeeded() ? item.origin : std::filesystem::path{};

    return swapped;
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
