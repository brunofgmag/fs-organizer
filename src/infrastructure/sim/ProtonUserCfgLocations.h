#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_USER_CFG_LOCATIONS_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_USER_CFG_LOCATIONS_H

#include <filesystem>
#include <vector>

#include "infrastructure/sim/UserCfgLocation.h"

[[nodiscard]] std::vector<std::filesystem::path> SteamLibraries();

[[nodiscard]] std::vector<UserCfgLocation> ProtonUserCfgLocations(const std::vector<std::filesystem::path>& libraries);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_USER_CFG_LOCATIONS_H
