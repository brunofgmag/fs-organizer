#include "domain/importing/ImportEngine.h"

#include <algorithm>
#include <numeric>
#include <ranges>

#include "domain/support/PathUtils.h"

namespace
{
    std::uintmax_t TotalSizeOf(const std::vector<FileFingerprint>& files)
    {
        const auto sizes = files | std::views::transform(&FileFingerprint::size);

        return std::accumulate(sizes.begin(), sizes.end(), std::uintmax_t{0});
    }

    std::filesystem::path StagingPathFor(const std::filesystem::path& target)
    {
        std::filesystem::path staging = target;
        staging += kStagingSuffix;

        return staging;
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
                           const LinkType linkType)
    : filesystemProbe_(filesystemProbe), files_(files), linking_(linking), linkType_(linkType)
{
}

ImportOutcome ImportEngine::Import(const SimulatorProfile& profile,
                                   const ImportRequest& request,
                                   const std::function<bool(const CopyProgress&)>& onProgress) const
{
    if (!IsUnderADestination(profile, request.source))
    {
        return ImportOutcome::Stopped(ImportResult::SourceIsNotUnderADestination);
    }

    if (filesystemProbe_.IsReparsePoint(request.source))
    {
        return ImportOutcome::Stopped(ImportResult::SourceIsAReparsePoint);
    }

    const std::vector<FileFingerprint> source = filesystemProbe_.FingerprintTree(request.source);

    if (const ImportOutcome room = CheckFreeSpace(request.target, TotalSizeOf(source));
        !room.Succeeded())
    {
        return room;
    }

    const std::filesystem::path staging = StagingPathFor(request.target);

    if (const ImportOutcome copy = CopyToStaging(request.source, staging, onProgress);
        !copy.Succeeded())
    {
        return copy;
    }

    if (!FingerprintsMatch(source, filesystemProbe_.FingerprintTree(staging)))
    {
        return ImportOutcome::Stopped(ImportResult::VerificationFailed);
    }

    if (!files_.Move(staging, request.target))
    {
        return ImportOutcome::Stopped(ImportResult::CouldNotMoveIntoPlace);
    }

    if (!files_.RemoveTree(request.source))
    {
        return ImportOutcome::Stopped(ImportResult::CouldNotRemoveSource);
    }

    if (!linking_.Enable(Addon{request.target}, request.source.parent_path(), linkType_).Succeeded())
    {
        return ImportOutcome::Stopped(ImportResult::CouldNotCreateLink);
    }

    return ImportOutcome::Completed();
}

ImportOutcome ImportEngine::CheckFreeSpace(const std::filesystem::path& target, const std::uintmax_t sourceSize) const
{
    const std::uintmax_t needed = sourceSize + kFreeSpaceMargin;

    const std::optional<std::uintmax_t> free = filesystemProbe_.FreeSpaceOn(target);
    if (!free.has_value())
    {
        return ImportOutcome::Stopped(ImportResult::CouldNotCheckFreeSpace);
    }

    if (*free < needed)
    {
        return ImportOutcome::Stopped(ImportResult::NotEnoughFreeSpace);
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
        return ImportOutcome::Stopped(ImportResult::Cancelled);
    case CopyOutcome::Failed:
        return ImportOutcome::Stopped(ImportResult::CouldNotCopy);
    case CopyOutcome::Completed:
        break;
    }

    return ImportOutcome::Completed();
}
