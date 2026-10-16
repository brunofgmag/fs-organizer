#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_PREFIXES_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_PREFIXES_H

#include <filesystem>
#include <vector>

#include "infrastructure/sim/ProtonPrefix.h"

[[nodiscard]] std::vector<std::filesystem::path> SteamRoots();

[[nodiscard]] std::vector<std::filesystem::path> SteamLibraries(const std::vector<std::filesystem::path>& roots);

[[nodiscard]] std::vector<ProtonPrefix> ProtonPrefixes(const std::vector<std::filesystem::path>& libraries);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_PREFIXES_H
