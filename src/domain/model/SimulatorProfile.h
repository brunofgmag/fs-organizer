#ifndef FS_ORGANIZER_DOMAIN_MODEL_SIMULATOR_PROFILE_H
#define FS_ORGANIZER_DOMAIN_MODEL_SIMULATOR_PROFILE_H

#include <filesystem>
#include <string>
#include <vector>

#include "domain/model/Library.h"
#include "domain/model/LibraryId.h"

enum class SimulatorVariant : int
{
    MSFS2020 = 0,
    MSFS2024 = 1,
};

struct DestinationOverride
{
    LibraryId libraryId;
    std::filesystem::path relativePath;
    std::filesystem::path destination;
};

struct SimulatorCandidate
{
    SimulatorVariant variant = SimulatorVariant::MSFS2024;
    std::filesystem::path packagesPath;
    std::vector<std::filesystem::path> destinations;
};

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
