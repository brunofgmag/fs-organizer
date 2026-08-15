#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_LOADING_REPORT_LOCATIONS_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_LOADING_REPORT_LOCATIONS_H

#include <filesystem>
#include <vector>

#include "domain/ports/FilesystemProbe.h"
#include "infrastructure/sim/UserCfgLocation.h"

struct LoadingReportLocation
{
    SimulatorVariant variant = SimulatorVariant::MSFS2024;
    std::filesystem::path filePath;
};

[[nodiscard]] std::vector<LoadingReportLocation>
LoadingReportLocations(const std::vector<UserCfgLocation>& userCfgLocations, const FilesystemProbe& filesystemProbe);

[[nodiscard]] std::filesystem::path LoadingReportOf(const std::vector<LoadingReportLocation>& locations,
                                                    SimulatorVariant variant);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_LOADING_REPORT_LOCATIONS_H
