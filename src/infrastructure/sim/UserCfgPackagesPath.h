#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_USER_CFG_PACKAGES_PATH_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_USER_CFG_PACKAGES_PATH_H

#include <filesystem>
#include <optional>

[[nodiscard]] std::optional<std::filesystem::path> InstalledPackagesPathIn(const std::filesystem::path& userCfg);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_USER_CFG_PACKAGES_PATH_H
