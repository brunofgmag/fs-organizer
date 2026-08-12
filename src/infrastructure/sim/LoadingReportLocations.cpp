#include "infrastructure/sim/LoadingReportLocations.h"

#include <optional>
#include <string>
#include <string_view>

#include "domain/support/PathUtils.h"

namespace
{
    constexpr std::string_view kLoadingReportName = "Report-loading.toml";

    [[nodiscard]] std::optional<std::filesystem::path> TheReportBeside(const std::filesystem::path& base,
                                                                       const FilesystemProbe& filesystemProbe)
    {
        const std::string wanted = ComparableFileName(base / kLoadingReportName);

        for (const std::filesystem::path& entry : filesystemProbe.ChildFiles(base))
        {
            if (ComparableFileName(entry) == wanted)
            {
                return entry;
            }
        }

        return std::nullopt;
    }
}

std::vector<LoadingReportLocation> LoadingReportLocations(const std::vector<UserCfgLocation>& userCfgLocations,
                                                          const FilesystemProbe& filesystemProbe)
{
    std::vector<LoadingReportLocation> found;

    for (const UserCfgLocation& location : userCfgLocations)
    {
        if (!filesystemProbe.EntryExistsWithoutFollowingLinks(location.configPath))
        {
            continue;
        }

        const std::optional<std::filesystem::path> file =
            TheReportBeside(location.configPath.parent_path(), filesystemProbe);

        if (file.has_value())
        {
            found.push_back({.variant = location.variant, .filePath = *file});
        }
    }

    return found;
}

std::filesystem::path LoadingReportOf(const std::vector<LoadingReportLocation>& locations,
                                      const SimulatorVariant variant)
{
    for (const LoadingReportLocation& location : locations)
    {
        if (location.variant == variant)
        {
            return location.filePath;
        }
    }

    return {};
}
