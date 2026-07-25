#ifndef FS_ORGANIZER_DOMAIN_MODEL_SIMULATOR_PROFILE_H
#define FS_ORGANIZER_DOMAIN_MODEL_SIMULATOR_PROFILE_H

#include <filesystem>
#include <string>
#include <vector>

#include "domain/model/DestinationOverride.h"
#include "domain/model/Library.h"
#include "domain/model/SimulatorVariant.h"

struct SimulatorProfile
{
    std::string id;
    SimulatorVariant variant = SimulatorVariant::MSFS2024;
    std::vector<std::filesystem::path> destinations;
    std::filesystem::path defaultDestination;
    std::vector<Library> libraries;
    std::vector<DestinationOverride> destinationOverrides;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_SIMULATOR_PROFILE_H
