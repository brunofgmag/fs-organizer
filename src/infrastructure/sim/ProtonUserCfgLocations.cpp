#include "infrastructure/sim/ProtonUserCfgLocations.h"

#include <algorithm>
#include <system_error>

#include "infrastructure/platform/LinuxKnownFolders.h"

namespace
{
    constexpr auto kMsfs2020AppId = "1250410";
    constexpr auto kMsfs2024AppId = "2537590";
    constexpr auto kProtonUser = "steamuser";

    std::filesystem::path RoamingAppDataOf(const std::filesystem::path& library, const char* appId)
    {
        return library / "steamapps" / "compatdata" / appId / "pfx" / "drive_c" / "users" / kProtonUser / "AppData"
            / "Roaming";
    }

    bool IsAlreadyIn(const std::vector<std::filesystem::path>& libraries, const std::filesystem::path& candidate)
    {
        std::error_code error;
        const std::filesystem::path resolved = std::filesystem::weakly_canonical(candidate, error);

        return std::ranges::any_of(libraries,
                                   [&resolved](const std::filesystem::path& known)
                                   {
                                       std::error_code ignored;

                                       return std::filesystem::weakly_canonical(known, ignored) == resolved;
                                   });
    }
}

std::vector<std::filesystem::path> SteamLibraries()
{
    std::vector<std::filesystem::path> libraries;

    for (const std::filesystem::path& candidate : {DataHomeFolder() / "Steam", HomeFolder() / ".steam" / "steam"})
    {
        if (!IsAlreadyIn(libraries, candidate))
        {
            libraries.push_back(candidate);
        }
    }

    return libraries;
}

std::vector<UserCfgLocation> ProtonUserCfgLocations(const std::vector<std::filesystem::path>& libraries)
{
    std::vector<UserCfgLocation> locations;

    for (const std::filesystem::path& library : libraries)
    {
        locations.push_back(
            {.variant = SimulatorVariant::MSFS2020,
             .configPath = RoamingAppDataOf(library, kMsfs2020AppId) / "Microsoft Flight Simulator" / "UserCfg.opt"});
        locations.push_back({.variant = SimulatorVariant::MSFS2024,
                             .configPath = RoamingAppDataOf(library, kMsfs2024AppId) / "Microsoft Flight Simulator 2024"
                                 / "UserCfg.opt"});
    }

    return locations;
}
