#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_LIST_LOCATIONS_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_LIST_LOCATIONS_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "domain/ports/FilesystemProbe.h"
#include "infrastructure/sim/UserCfgLocation.h"

struct ContentListLocation
{
    SimulatorVariant variant = SimulatorVariant::MSFS2024;
    std::filesystem::path listPath;
};

struct ChosenContentList
{
    std::filesystem::path listPath;
    std::string accountFolder;
};

[[nodiscard]] std::vector<ContentListLocation>
ContentListLocations(const std::vector<UserCfgLocation>& userCfgLocations, const FilesystemProbe& filesystemProbe);

[[nodiscard]] std::optional<ChosenContentList> ChooseContentList(const std::vector<ContentListLocation>& locations,
                                                                 SimulatorVariant variant);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_LIST_LOCATIONS_H
