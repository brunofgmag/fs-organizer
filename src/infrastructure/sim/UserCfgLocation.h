#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_USER_CFG_LOCATION_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_USER_CFG_LOCATION_H

#include <filesystem>

#include "domain/model/SimulatorProfile.h"

struct UserCfgLocation
{
    SimulatorVariant variant = SimulatorVariant::MSFS2024;
    std::filesystem::path configPath;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_USER_CFG_LOCATION_H
