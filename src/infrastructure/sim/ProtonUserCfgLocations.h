#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_USER_CFG_LOCATIONS_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_USER_CFG_LOCATIONS_H

#include <vector>

#include "infrastructure/sim/ProtonPrefix.h"
#include "infrastructure/sim/UserCfgLocation.h"

[[nodiscard]] std::vector<UserCfgLocation> ProtonUserCfgLocations(const std::vector<ProtonPrefix>& prefixes);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_USER_CFG_LOCATIONS_H
