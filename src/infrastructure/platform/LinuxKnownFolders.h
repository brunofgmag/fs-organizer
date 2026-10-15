#ifndef FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_LINUX_KNOWN_FOLDERS_H
#define FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_LINUX_KNOWN_FOLDERS_H

#include <filesystem>

#include "infrastructure/platform/AppFolders.h"

[[nodiscard]] std::filesystem::path HomeFolder();

[[nodiscard]] std::filesystem::path ConfigHomeFolder();

[[nodiscard]] std::filesystem::path DataHomeFolder();

[[nodiscard]] std::filesystem::path CacheHomeFolder();

[[nodiscard]] AppFolders LinuxAppFolders();

#endif // FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_LINUX_KNOWN_FOLDERS_H
