#ifndef FS_ORGANIZER_DOMAIN_MODEL_SIMULATOR_CANDIDATE_H
#define FS_ORGANIZER_DOMAIN_MODEL_SIMULATOR_CANDIDATE_H

#include <filesystem>
#include <vector>

#include "domain/model/SimulatorVariant.h"

struct SimulatorCandidate
{
    SimulatorVariant variant = SimulatorVariant::MSFS2024;
    std::filesystem::path packagesPath;
    std::vector<std::filesystem::path> destinations;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_SIMULATOR_CANDIDATE_H
