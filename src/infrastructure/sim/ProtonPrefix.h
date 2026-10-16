#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_PREFIX_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_PREFIX_H

#include <filesystem>

#include "domain/model/SimulatorProfile.h"

struct ProtonPrefix
{
    SimulatorVariant variant = SimulatorVariant::MSFS2024;
    std::filesystem::path path{};
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_PREFIX_H
