#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_LIST_LOCATIONS_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_LIST_LOCATIONS_H

#include <filesystem>
#include <vector>

#include "domain/ports/FilesystemProbe.h"
#include "infrastructure/sim/WindowsSimulatorLocator.h"

struct ContentListLocation
{
    SimulatorVariant variant = SimulatorVariant::MSFS2024;
    std::filesystem::path listPath;
};

[[nodiscard]] std::vector<ContentListLocation>
ContentListLocations(const std::vector<UserCfgLocation>& userCfgLocations, const FilesystemProbe& filesystemProbe);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_LIST_LOCATIONS_H
