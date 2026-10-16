#include "infrastructure/archive/FolderEntryPaths.h"

#include <utility>

#include "domain/support/PathSegment.h"
#include "domain/support/PathUtils.h"

namespace
{
    [[nodiscard]] std::vector<std::string> SegmentsOf(const std::string& entry)
    {
        std::vector<std::string> segments;
        std::size_t from = 0;

        for (std::size_t slash = entry.find('/'); slash != std::string::npos; slash = entry.find('/', from))
        {
            segments.push_back(entry.substr(from, slash - from));
            from = slash + 1;
        }

        segments.push_back(entry.substr(from));

        return segments;
    }
}

std::optional<std::filesystem::path> BelowTheFolder(const std::string& entry, const std::string& name)
{
    const std::vector<std::string> segments = SegmentsOf(entry);
    if (segments.front() != name)
    {
        return std::nullopt;
    }

    std::filesystem::path below;
    for (auto segment = segments.begin() + 1; segment != segments.end(); ++segment)
    {
        if (!PathSegment::From(*segment).has_value())
        {
            return std::nullopt;
        }

        below /= PathFromUtf8(*segment);
    }

    return below;
}

std::optional<std::vector<std::filesystem::path>> RelativePathsOf(const std::vector<ArchiveEntry>& listed,
                                                                  const std::string& name)
{
    std::vector<std::filesystem::path> relative;
    relative.reserve(listed.size());

    for (const ArchiveEntry& entry : listed)
    {
        std::optional<std::filesystem::path> below = BelowTheFolder(entry.path, name);
        if (!below.has_value() || entry.kind == ArchiveEntryKind::Other)
        {
            return std::nullopt;
        }

        relative.push_back(std::move(*below));
    }

    return relative;
}

std::uintmax_t BytesOfTheFiles(const std::vector<ArchiveEntry>& listed)
{
    std::uintmax_t total = 0;

    for (const ArchiveEntry& entry : listed)
    {
        total += entry.kind == ArchiveEntryKind::File ? entry.bytes : 0;
    }

    return total;
}
