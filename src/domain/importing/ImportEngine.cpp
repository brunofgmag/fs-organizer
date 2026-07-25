#include "domain/importing/ImportEngine.h"

#include <numeric>
#include <ranges>

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
}

ImportEngine::ImportEngine(const FilesystemProbe& filesystemProbe, FileOperations& files)
    : filesystemProbe_(filesystemProbe), files_(files)
{
}

ImportOutcome ImportEngine::Import(const SimulatorProfile&,
                                   const ImportRequest& request,
                                   const std::function<bool(const CopyProgress&)>& onProgress) const
{
    if (const ImportOutcome room = CheckFreeSpace(request); !room.Succeeded())
    {
        return room;
    }

    const std::filesystem::path staging = StagingPathFor(request.target);

    if (const ImportOutcome copy = CopyToStaging(request.source, staging, onProgress);
        !copy.Succeeded())
    {
        return copy;
    }

    return ImportOutcome::Completed();
}

ImportOutcome ImportEngine::CheckFreeSpace(const ImportRequest& request) const
{
    const std::uintmax_t needed =
        TotalSizeOf(filesystemProbe_.FingerprintTree(request.source)) + kFreeSpaceMargin;

    const std::optional<std::uintmax_t> free = filesystemProbe_.FreeSpaceOn(request.target);
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
