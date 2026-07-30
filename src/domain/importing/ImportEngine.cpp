#include "domain/importing/ImportEngine.h"

#include <algorithm>
#include <numeric>
#include <ranges>

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
                           const LinkingEngine& linking,
                           const OperationLog& log,
                           const LinkType linkType)
    : filesystemProbe_(filesystemProbe), files_(files), linking_(linking), log_(log), linkType_(linkType)
{
}

ImportOutcome ImportEngine::Import(const SimulatorProfile& profile,
                                   const ImportRequest& request,
                                   const std::function<bool(const CopyProgress&)>& onProgress,
                                   const std::function<void(OperationKind)>& onStep) const
{
    const auto announce = [&onStep](const OperationKind kind)
    {
        if (onStep)
        {
            onStep(kind);
        }
    };

    if (!IsUnderADestination(profile, request.source))
    {
        return ImportOutcome::Stopped(FileResult::SourceIsNotUnderADestination);
    }

    if (filesystemProbe_.IsReparsePoint(request.source))
    {
        return ImportOutcome::Stopped(FileResult::SourceIsAReparsePoint);
    }

    const std::optional<std::vector<FileFingerprint>> source = filesystemProbe_.FingerprintTree(request.source);
    if (!source.has_value())
    {
        return ImportOutcome::Stopped(FileResult::CouldNotReadTheSource);
    }

    if (const ImportOutcome room = CheckFreeSpace(request.category, TotalSizeOf(*source)); !room.Succeeded())
    {
        return room;
    }

    const std::filesystem::path target = request.Target();
    const std::filesystem::path staging = StagingPathFor(target);
    const AddonId addon = IdentityOf(profile, target);

    announce(OperationKind::ImportCopyToStaging);
    const ImportOutcome copy = CopyToStaging(request.source, staging, onProgress);
    RecordStep(addon, OperationKind::ImportCopyToStaging, request.source, staging, copy.Result());
    if (!copy.Succeeded())
    {
        return copy;
    }

    announce(OperationKind::ImportVerifyStaging);
    const std::optional<std::vector<FileFingerprint>> copied = filesystemProbe_.FingerprintTree(staging);
    const bool verified = copied.has_value() && FingerprintsMatch(*source, *copied);
    RecordStep(addon, OperationKind::ImportVerifyStaging, staging, target,
               verified ? FileResult::Completed : FileResult::VerificationFailed);
    if (!verified)
    {
        return ImportOutcome::Stopped(FileResult::VerificationFailed);
    }

    announce(OperationKind::ImportMoveIntoPlace);
    const bool moved = files_.Move(staging, target);
    RecordStep(addon, OperationKind::ImportMoveIntoPlace, staging, target,
               moved ? FileResult::Completed : FileResult::CouldNotMoveIntoPlace);
    if (!moved)
    {
        return ImportOutcome::Stopped(FileResult::CouldNotMoveIntoPlace);
    }

    announce(OperationKind::ImportRemoveSource);
    const bool removed = files_.RemoveTree(request.source);
    RecordStep(addon, OperationKind::ImportRemoveSource, request.source, target,
               removed ? FileResult::Completed : FileResult::CouldNotRemoveSource);
    if (!removed)
    {
        return ImportOutcome::Stopped(FileResult::CouldNotRemoveSource);
    }

    announce(OperationKind::EnableAddon);
    const LinkOutcome link = linking_.Enable(Addon{target}, request.source.parent_path(), linkType_);
    log_.RecordLink(OperationKind::EnableAddon, addon, target, request.source, link.Failure());
    if (!link.Succeeded())
    {
        return ImportOutcome::Stopped(FileResult::CouldNotCreateLink);
    }

    return ImportOutcome::Completed();
}

void ImportEngine::RecordStep(const AddonId& addon,
                              const OperationKind kind,
                              const std::filesystem::path& source,
                              const std::filesystem::path& target,
                              const FileResult result) const
{
    log_.RecordImport(kind, addon, source, target, result);
}

ImportOutcome ImportEngine::CheckFreeSpace(const std::filesystem::path& category, const std::uintmax_t sourceSize) const
{
    const std::uintmax_t needed = sourceSize + kFreeSpaceMargin;

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
