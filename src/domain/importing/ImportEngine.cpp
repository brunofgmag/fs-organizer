#include "domain/importing/ImportEngine.h"

#include <algorithm>
#include <numeric>
#include <ranges>

#include "domain/importing/ExternalSidecar.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    std::uintmax_t TotalSizeOf(const std::vector<FileFingerprint>& files)
    {
        const auto sizes = files | std::views::transform(&FileFingerprint::size);

        return std::accumulate(sizes.begin(), sizes.end(), std::uintmax_t{0});
    }

    bool IsUnderADestination(const SimulatorProfile& profile, const std::filesystem::path& path)
    {
        return std::ranges::any_of(profile.destinations,
                                   [&path](const std::filesystem::path& destination)
                                   {
                                       return PathIsInside(path, destination)
                                           && ComparablePath(path) != ComparablePath(destination);
                                   });
    }

    void Announce(const std::function<void(OperationKind)>& onStep, const OperationKind kind)
    {
        if (onStep)
        {
            onStep(kind);
        }
    }

    bool FingerprintsMatch(std::vector<FileFingerprint> left, std::vector<FileFingerprint> right)
    {
        const auto byPath = [](const FileFingerprint& a, const FileFingerprint& b)
        {
            return a.relativePath < b.relativePath;
        };

        std::ranges::sort(left, byPath);
        std::ranges::sort(right, byPath);

        return std::ranges::equal(left, right,
                                  [](const FileFingerprint& a, const FileFingerprint& b)
                                  {
                                      return a.relativePath == b.relativePath && a.size == b.size;
                                  });
    }
}

ImportEngine::ImportEngine(const FilesystemProbe& filesystemProbe,
                           FileOperations& files,
                           SidecarStore& sidecars,
                           const LinkingEngine& linking,
                           const OperationLog& log,
                           const LinkType linkType)
    : filesystemProbe_(filesystemProbe),
      files_(files),
      sidecars_(sidecars),
      linking_(linking),
      log_(log),
      linkType_(linkType)
{
}

void ImportEngine::UseLinkType(const LinkType linkType)
{
    linkType_ = linkType;
}

ImportOutcome ImportEngine::Import(const SimulatorProfile& profile,
                                   const ImportRequest& request,
                                   const std::function<bool(const CopyProgress&)>& onProgress,
                                   const std::function<void(OperationKind)>& onStep) const
{
    if (const ImportOutcome refusal = CheckTheSource(profile, request); !refusal.Succeeded())
    {
        return refusal;
    }

    const MeasuredSource source = MeasureTheSource(request.Bytes(), request.category);
    if (!source.outcome.Succeeded())
    {
        return source.outcome;
    }

    const std::filesystem::path target = request.Target();
    const std::filesystem::path staging = StagingPathFor(target);
    const AddonId addon = IdentityOf(profile, target);

    if (request.CameFromAnotherProgram())
    {
        RecordStep(addon, OperationKind::ImportFromAnotherProgram, request.source, staging, FileResult::Completed);
    }

    if (const ImportOutcome copied = CopyAndVerify(addon, request.Bytes(), target, source.files, onProgress, onStep);
        !copied.Succeeded())
    {
        return copied;
    }

    if (request.CameFromAnotherProgram())
    {
        if (const ImportOutcome refusal = PrepareTheOtherProgramsFolder(request.externalSource, target);
            !refusal.Succeeded())
        {
            static_cast<void>(files_.RemoveTree(staging));
            return refusal;
        }
    }

    if (const ImportOutcome placed = PutIntoPlace(addon, target, onStep); !placed.Succeeded())
    {
        return placed;
    }

    Announce(onStep, OperationKind::ImportRemoveSource);
    if (request.CameFromAnotherProgram())
    {
        return TakeOverTheOtherProgramsFolder(addon, request, target);
    }

    const bool removed = files_.RemoveTree(request.source);
    RecordStep(addon, OperationKind::ImportRemoveSource, request.source, target,
               removed ? FileResult::Completed : FileResult::CouldNotRemoveSource);
    if (!removed)
    {
        return ImportOutcome::Stopped(FileResult::CouldNotRemoveSource);
    }

    Announce(onStep, OperationKind::EnableAddon);
    const LinkOutcome link = linking_.Enable(Addon{.folderPath = target}, request.source.parent_path(), linkType_);
    log_.RecordLink(OperationKind::EnableAddon, addon, target, request.source, link.Failure());
    if (!link.Succeeded())
    {
        return ImportOutcome::Stopped(FileResult::CouldNotCreateLink);
    }

    return ImportOutcome::Completed();
}

ImportOutcome ImportEngine::GiveBack(const SimulatorProfile& profile,
                                     const GiveBackRequest& request,
                                     const std::function<bool(const CopyProgress&)>& onProgress,
                                     const std::function<void(OperationKind)>& onStep) const
{
    if (const ImportOutcome refusal = CheckTheFolderWeAreGivingBack(request); !refusal.Succeeded())
    {
        return refusal;
    }

    const MeasuredSource source = MeasureTheSource(request.addonFolder, request.externalPath.parent_path());
    if (!source.outcome.Succeeded())
    {
        return source.outcome;
    }

    const std::filesystem::path staging = StagingPathFor(request.externalPath);
    const AddonId addon = IdentityOf(profile, request.addonFolder);

    RecordStep(addon, OperationKind::GiveBackToAnotherProgram, request.addonFolder, request.externalPath,
               FileResult::Completed);

    const ImportOutcome copied =
        CopyAndVerify(addon, request.addonFolder, request.externalPath, source.files, onProgress, onStep);
    if (!copied.Succeeded())
    {
        if (copied.Result() == FileResult::VerificationFailed)
        {
            static_cast<void>(files_.RemoveTree(staging));
        }

        return copied;
    }

    return TheOtherProgramTakesItsFolderBack(addon, request, staging, onStep);
}

ImportOutcome ImportEngine::Quarantine(const QuarantineRequest& request,
                                       const std::function<bool(const CopyProgress&)>& onProgress,
                                       const std::function<void(OperationKind)>& onStep) const
{
    const auto conclude = [this, &request](const ImportOutcome outcome)
    {
        RecordStep(request.addon, request.kind, request.loser, request.quarantine, outcome.Result());

        return outcome;
    };

    if (OnTheSameVolume(request.loser, request.quarantine))
    {
        return conclude(files_.Move(request.loser, request.quarantine)
                            ? ImportOutcome::Completed()
                            : ImportOutcome::Stopped(FileResult::CouldNotQuarantine));
    }

    const MeasuredSource source = MeasureTheSource(request.loser, request.quarantine.parent_path());
    if (!source.outcome.Succeeded())
    {
        return conclude(source.outcome);
    }

    if (const ImportOutcome copied =
            CopyAndVerify(request.addon, request.loser, request.quarantine, source.files, onProgress, onStep);
        !copied.Succeeded())
    {
        return conclude(copied);
    }

    if (const ImportOutcome placed = PutIntoPlace(request.addon, request.quarantine, onStep); !placed.Succeeded())
    {
        return conclude(placed);
    }

    Announce(onStep, OperationKind::ImportRemoveSource);
    const bool removed = files_.RemoveTree(request.loser);
    RecordStep(request.addon, OperationKind::ImportRemoveSource, request.loser, request.quarantine,
               removed ? FileResult::Completed : FileResult::CouldNotRemoveSource);

    return conclude(removed ? ImportOutcome::Completed() : ImportOutcome::Stopped(FileResult::CouldNotRemoveSource));
}

MeasuredSource ImportEngine::MeasureTheSource(const std::filesystem::path& source,
                                              const std::filesystem::path& roomOn) const
{
    const std::optional<TreeFingerprint> walked = filesystemProbe_.FingerprintTree(source);
    if (!walked.has_value())
    {
        return MeasuredSource{.outcome = ImportOutcome::Stopped(FileResult::CouldNotReadTheSource)};
    }

    return MeasuredSource{.outcome = CheckFreeSpace(roomOn, TotalSizeOf(walked->files), walked->files.size()),
                          .files = walked->files};
}

ImportOutcome ImportEngine::CopyAndVerify(const AddonId& addon,
                                          const std::filesystem::path& source,
                                          const std::filesystem::path& target,
                                          const std::vector<FileFingerprint>& expected,
                                          const std::function<bool(const CopyProgress&)>& onProgress,
                                          const std::function<void(OperationKind)>& onStep) const
{
    const std::filesystem::path staging = StagingPathFor(target);

    Announce(onStep, OperationKind::ImportCopyToStaging);
    const ImportOutcome copy = CopyToStaging(source, staging, onProgress);
    RecordStep(addon, OperationKind::ImportCopyToStaging, source, staging, copy.Result());
    if (!copy.Succeeded())
    {
        return copy;
    }

    Announce(onStep, OperationKind::ImportVerifyStaging);
    const std::optional<TreeFingerprint> copied = filesystemProbe_.FingerprintTree(staging);
    const bool verified = copied.has_value() && FingerprintsMatch(expected, copied->files);
    RecordStep(addon, OperationKind::ImportVerifyStaging, staging, target,
               verified ? FileResult::Completed : FileResult::VerificationFailed);

    return verified ? ImportOutcome::Completed() : ImportOutcome::Stopped(FileResult::VerificationFailed);
}

ImportOutcome ImportEngine::PutIntoPlace(const AddonId& addon,
                                         const std::filesystem::path& target,
                                         const std::function<void(OperationKind)>& onStep) const
{
    const std::filesystem::path staging = StagingPathFor(target);

    Announce(onStep, OperationKind::ImportMoveIntoPlace);
    const bool moved = files_.Move(staging, target);
    RecordStep(addon, OperationKind::ImportMoveIntoPlace, staging, target,
               moved ? FileResult::Completed : FileResult::CouldNotMoveIntoPlace);

    return moved ? ImportOutcome::Completed() : ImportOutcome::Stopped(FileResult::CouldNotMoveIntoPlace);
}

ImportOutcome ImportEngine::CheckTheFolderWeAreGivingBack(const GiveBackRequest& request) const
{
    if (request.externalPath.empty() || !filesystemProbe_.PhysicalDirectoryExists(request.addonFolder))
    {
        return ImportOutcome::Stopped(FileResult::CouldNotReadTheSource);
    }

    if (const WriteAccess access = filesystemProbe_.ProbeWritable(request.externalPath.parent_path());
        !ItAcceptsWrites(access))
    {
        return ImportOutcome::Stopped(FileResult::CannotWriteInTheOtherProgramsFolder, access);
    }

    if (!filesystemProbe_.EntryExistsWithoutFollowingLinks(request.externalPath))
    {
        return ImportOutcome::Completed();
    }

    const std::optional<std::filesystem::path> pointsAt = linking_.PointsAt(request.externalPath);

    return pointsAt.has_value() && ComparablePath(*pointsAt) == ComparablePath(request.addonFolder)
        ? ImportOutcome::Completed()
        : ImportOutcome::Stopped(FileResult::TheDiskDisagreesWithTheScan);
}

ImportOutcome ImportEngine::TheOtherProgramTakesItsFolderBack(const AddonId& addon,
                                                              const GiveBackRequest& request,
                                                              const std::filesystem::path& staging,
                                                              const std::function<void(OperationKind)>& onStep) const
{
    if (filesystemProbe_.EntryExistsWithoutFollowingLinks(request.externalPath))
    {
        const LinkOutcome unlinked = linking_.Disable(request.externalPath);
        log_.RecordLink(OperationKind::DisableAddon, addon, request.addonFolder, request.externalPath,
                        unlinked.Failure());
        if (!unlinked.Succeeded())
        {
            static_cast<void>(files_.RemoveTree(staging));
            return ImportOutcome::Stopped(FileResult::CouldNotRemoveTheLink);
        }
    }

    if (const ImportOutcome placed = PutIntoPlace(addon, request.externalPath, onStep); !placed.Succeeded())
    {
        return placed;
    }

    if (const ImportOutcome repointed = RepointTheLinks(addon, request); !repointed.Succeeded())
    {
        return repointed;
    }

    Announce(onStep, OperationKind::ImportRemoveSource);

    const bool removed = files_.RemoveTree(request.addonFolder);
    RecordStep(addon, OperationKind::ImportRemoveSource, request.addonFolder, request.externalPath,
               removed ? FileResult::Completed : FileResult::CouldNotRemoveSource);

    return removed ? ImportOutcome::Completed() : ImportOutcome::Stopped(FileResult::CouldNotRemoveSource);
}

ImportOutcome ImportEngine::RepointTheLinks(const AddonId& addon, const GiveBackRequest& request) const
{
    for (const std::filesystem::path& link : request.links)
    {
        const LinkOutcome off = linking_.Disable(link);
        log_.RecordLink(OperationKind::DisableAddon, addon, request.addonFolder, link, off.Failure());
        if (!off.Succeeded())
        {
            return ImportOutcome::Stopped(FileResult::CouldNotRemoveTheLink);
        }

        const LinkOutcome on = linking_.LinkAt(link, Addon{.folderPath = request.externalPath}, linkType_);
        log_.RecordLink(OperationKind::EnableAddon, addon, request.externalPath, link, on.Failure());
        if (!on.Succeeded())
        {
            return ImportOutcome::Stopped(FileResult::CouldNotCreateLink);
        }
    }

    return ImportOutcome::Completed();
}

ImportOutcome ImportEngine::CheckTheSource(const SimulatorProfile& profile, const ImportRequest& request) const
{
    if (!IsUnderADestination(profile, request.source))
    {
        return ImportOutcome::Stopped(FileResult::SourceIsNotUnderADestination);
    }

    if (!request.CameFromAnotherProgram())
    {
        return filesystemProbe_.IsReparsePoint(request.source)
            ? ImportOutcome::Stopped(FileResult::SourceIsAReparsePoint)
            : ImportOutcome::Completed();
    }

    const std::optional<std::filesystem::path> pointsAt = linking_.PointsAt(request.source);
    if (!pointsAt.has_value() || ComparablePath(*pointsAt) != ComparablePath(request.externalSource))
    {
        return ImportOutcome::Stopped(FileResult::TheDiskDisagreesWithTheScan);
    }

    return ImportOutcome::Completed();
}

ImportOutcome ImportEngine::PrepareTheOtherProgramsFolder(const std::filesystem::path& externalSource,
                                                          const std::filesystem::path& target) const
{
    if (const WriteAccess access = filesystemProbe_.ProbeWritable(externalSource.parent_path());
        !ItAcceptsWrites(access))
    {
        return ImportOutcome::Stopped(FileResult::CannotWriteInTheOtherProgramsFolder, access);
    }

    if (!sidecars_.Write(ExternalSidecarPathFor(target), TextOfTheExternalOrigin(externalSource)))
    {
        return ImportOutcome::Stopped(FileResult::CouldNotRecordTheOrigin);
    }

    return ImportOutcome::Completed();
}

ImportOutcome ImportEngine::TakeOverTheOtherProgramsFolder(const AddonId& addon,
                                                           const ImportRequest& request,
                                                           const std::filesystem::path& target) const
{
    const std::filesystem::path aside = SwapSlotFor(request.externalSource);

    const bool movedAside = files_.Move(request.externalSource, aside);
    RecordStep(addon, OperationKind::ImportRemoveSource, request.externalSource, target,
               movedAside ? FileResult::Completed : FileResult::CouldNotRemoveSource);
    if (!movedAside)
    {
        return ImportOutcome::Stopped(FileResult::CouldNotRemoveSource);
    }

    const LinkOutcome standIn = linking_.LinkAt(request.externalSource, Addon{.folderPath = target}, linkType_);
    log_.RecordLink(OperationKind::LinkTheOtherProgramsFolder, addon, target, request.externalSource,
                    standIn.Failure());
    if (!standIn.Succeeded())
    {
        static_cast<void>(files_.Move(aside, request.externalSource));
        return ImportOutcome::Stopped(FileResult::CouldNotCreateLink);
    }

    const LinkOutcome unlinked = linking_.Disable(request.source);
    log_.RecordLink(OperationKind::DisableAddon, addon, target, request.source, unlinked.Failure());
    if (!unlinked.Succeeded())
    {
        return ImportOutcome::Stopped(FileResult::CouldNotRemoveTheLink);
    }

    const LinkOutcome link = linking_.Enable(Addon{.folderPath = target}, request.source.parent_path(), linkType_);
    log_.RecordLink(OperationKind::EnableAddon, addon, target, request.source, link.Failure());
    if (!link.Succeeded())
    {
        return ImportOutcome::Stopped(FileResult::CouldNotCreateLink);
    }

    const bool removed = files_.RemoveTree(aside);
    RecordStep(addon, OperationKind::ImportRemoveSource, aside, target,
               removed ? FileResult::Completed : FileResult::CouldNotRemoveSource);

    return removed ? ImportOutcome::Completed() : ImportOutcome::Stopped(FileResult::CouldNotRemoveSource);
}

void ImportEngine::RecordStep(const AddonId& addon,
                              const OperationKind kind,
                              const std::filesystem::path& source,
                              const std::filesystem::path& target,
                              const FileResult result) const
{
    log_.RecordImport(kind, addon, source, target, result);
}

ImportOutcome ImportEngine::CheckFreeSpace(const std::filesystem::path& category,
                                           const std::uintmax_t sourceSize,
                                           const std::size_t files) const
{
    const std::uintmax_t needed = FreeSpaceNeededFor(sourceSize, files);

    const std::optional<std::uintmax_t> free = filesystemProbe_.FreeSpaceOn(category);
    if (!free.has_value())
    {
        return ImportOutcome::Stopped(FileResult::CouldNotCheckFreeSpace);
    }

    if (*free < needed)
    {
        return ImportOutcome::Stopped(FileResult::NotEnoughFreeSpace);
    }

    return ImportOutcome::Completed();
}

ImportOutcome ImportEngine::CopyToStaging(const std::filesystem::path& source,
                                          const std::filesystem::path& staging,
                                          const std::function<bool(const CopyProgress&)>& onProgress) const
{
    switch (files_.CopyTree(source, staging, onProgress))
    {
    case CopyOutcome::Cancelled:
        static_cast<void>(files_.RemoveTree(staging));
        return ImportOutcome::Stopped(FileResult::Cancelled);
    case CopyOutcome::Failed: return ImportOutcome::Stopped(FileResult::CouldNotCopy);
    case CopyOutcome::Completed: break;
    }

    return ImportOutcome::Completed();
}
