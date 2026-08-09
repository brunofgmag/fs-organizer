#include "infrastructure/sim/StartupFileLocations.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "domain/support/PathUtils.h"

namespace
{
    constexpr std::string_view kStartupFileNames[] = {"EXE.xml", "exe.xml"};
    constexpr std::string_view kBackupSuffix = ".fsorg-backup";

    [[nodiscard]] std::optional<std::filesystem::path> TheStartupFileBeside(const std::filesystem::path& base,
                                                                            const FilesystemProbe& filesystemProbe)
    {
        const std::vector<std::filesystem::path> beside = filesystemProbe.ChildFiles(base);

        for (const std::string_view name : kStartupFileNames)
        {
            const std::string wanted = ComparableFileName(base / name);

            for (const std::filesystem::path& entry : beside)
            {
                if (ComparableFileName(entry) == wanted)
                {
                    return entry;
                }
            }
        }

        return std::nullopt;
    }
}

std::vector<StartupFileLocation> StartupFileLocations(const std::vector<UserCfgLocation>& userCfgLocations,
                                                      const FilesystemProbe& filesystemProbe)
{
    std::vector<StartupFileLocation> found;

    for (const UserCfgLocation& location : userCfgLocations)
    {
        if (!filesystemProbe.EntryExistsWithoutFollowingLinks(location.configPath))
        {
            continue;
        }

        const std::optional<std::filesystem::path> file =
            TheStartupFileBeside(location.configPath.parent_path(), filesystemProbe);

        if (file.has_value())
        {
            found.push_back({.variant = location.variant, .filePath = *file});
        }
    }

    return found;
}

std::filesystem::path StartupFileOf(const std::vector<StartupFileLocation>& locations, const SimulatorVariant variant)
{
    for (const StartupFileLocation& location : locations)
    {
        if (location.variant == variant)
        {
            return location.filePath;
        }
    }

    return {};
}

std::filesystem::path BackupOfStartupFile(const std::filesystem::path& filePath)
{
    std::filesystem::path backup = filePath;
    backup += kBackupSuffix;

    return backup;
}
