#include "infrastructure/sim/WindowsUserCfgLocations.h"

#include "infrastructure/platform/WindowsKnownFolders.h"

std::vector<UserCfgLocation> WindowsUserCfgLocations()
{
    const std::filesystem::path roaming = RoamingAppDataFolder();
    const std::filesystem::path local = LocalAppDataFolder();

    return {
        {SimulatorVariant::MSFS2020,
         local / "Packages" / "Microsoft.FlightSimulator_8wekyb3d8bbwe" / "LocalCache" / "UserCfg.opt"},
        {SimulatorVariant::MSFS2024,
         local / "Packages" / "Microsoft.Limitless_8wekyb3d8bbwe" / "LocalCache" / "UserCfg.opt"},
        {SimulatorVariant::MSFS2020, roaming / "Microsoft Flight Simulator" / "UserCfg.opt"},
        {SimulatorVariant::MSFS2024, roaming / "Microsoft Flight Simulator 2024" / "UserCfg.opt"},
    };
}
