#include "infrastructure/sim/WindowsUserCfgLocations.h"

#include "infrastructure/platform/WindowsKnownFolders.h"

std::vector<UserCfgLocation> WindowsUserCfgLocations()
{
    const std::filesystem::path roaming = RoamingAppDataFolder();
    const std::filesystem::path local = LocalAppDataFolder();

    return {
        {.variant = SimulatorVariant::MSFS2020,
         .configPath = local / "Packages" / "Microsoft.FlightSimulator_8wekyb3d8bbwe" / "LocalCache" / "UserCfg.opt"},
        {.variant = SimulatorVariant::MSFS2024,
         .configPath = local / "Packages" / "Microsoft.Limitless_8wekyb3d8bbwe" / "LocalCache" / "UserCfg.opt"},
        {.variant = SimulatorVariant::MSFS2020, .configPath = roaming / "Microsoft Flight Simulator" / "UserCfg.opt"},
        {.variant = SimulatorVariant::MSFS2024,
         .configPath = roaming / "Microsoft Flight Simulator 2024" / "UserCfg.opt"},
    };
}
