#ifndef FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_MOUNT_POINTS_H
#define FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_MOUNT_POINTS_H

#include <filesystem>
#include <string>
#include <vector>

[[nodiscard]] std::vector<std::filesystem::path> MountedPointsIn(const std::string& mountInfo);

[[nodiscard]] std::vector<std::filesystem::path> ConfiguredPointsIn(const std::string& fstab);

[[nodiscard]] bool TheMountIsMissing(const std::filesystem::path& path,
                                     const std::vector<std::filesystem::path>& mounted,
                                     const std::vector<std::filesystem::path>& configured);

#endif // FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_MOUNT_POINTS_H
