#include "domain/importing/FingerprintMeasures.h"

#include <algorithm>
#include <numeric>
#include <optional>
#include <ranges>

#include "domain/importing/ImportEngine.h"

std::uintmax_t TotalSizeOf(const std::vector<FileFingerprint>& files)
{
    const auto sizes = files | std::views::transform(&FileFingerprint::size);

    return std::accumulate(sizes.begin(), sizes.end(), std::uintmax_t{0});
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

FileResult RoomFor(const FilesystemProbe& probe,
                   const std::filesystem::path& folder,
                   const std::uintmax_t bytes,
                   const std::size_t files)
{
    const std::optional<std::uintmax_t> free = probe.FreeSpaceOn(folder);
    if (!free.has_value())
    {
        return FileResult::CouldNotCheckFreeSpace;
    }

    if (*free < FreeSpaceNeededFor(bytes, files))
    {
        return FileResult::NotEnoughFreeSpace;
    }

    return FileResult::Completed;
}
