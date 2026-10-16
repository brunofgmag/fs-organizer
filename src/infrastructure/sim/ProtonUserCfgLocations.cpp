#include "infrastructure/sim/ProtonUserCfgLocations.h"

namespace
{
    constexpr auto kProtonUser = "steamuser";
    constexpr auto kUserCfgName = "UserCfg.opt";

    const char* DataFolderOf(const SimulatorVariant variant)
    {
        switch (variant)
        {
        case SimulatorVariant::MSFS2020: return "Microsoft Flight Simulator";
        case SimulatorVariant::MSFS2024: return "Microsoft Flight Simulator 2024";
        }

        return "";
    }
}

std::vector<UserCfgLocation> ProtonUserCfgLocations(const std::vector<ProtonPrefix>& prefixes)
{
    std::vector<UserCfgLocation> locations;

    for (const ProtonPrefix& prefix : prefixes)
    {
        locations.push_back({.variant = prefix.variant,
                             .configPath = prefix.path / "drive_c" / "users" / kProtonUser / "AppData" / "Roaming"
                                 / DataFolderOf(prefix.variant) / kUserCfgName});
    }

    return locations;
}
