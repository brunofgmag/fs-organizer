#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_STARTUP_FILE_LOCATIONS_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_STARTUP_FILE_LOCATIONS_H

#include <filesystem>
#include <vector>

#include "domain/ports/FilesystemProbe.h"
#include "infrastructure/sim/UserCfgLocation.h"

struct StartupFileLocation
{
    SimulatorVariant variant = SimulatorVariant::MSFS2024;
    std::filesystem::path filePath;
};

[[nodiscard]] std::vector<StartupFileLocation>
StartupFileLocations(const std::vector<UserCfgLocation>& userCfgLocations, const FilesystemProbe& filesystemProbe);

[[nodiscard]] std::filesystem::path BackupOfStartupFile(const std::filesystem::path& filePath);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_STARTUP_FILE_LOCATIONS_H
