#ifndef FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_WINDOWS_KNOWN_FOLDERS_H
#define FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_WINDOWS_KNOWN_FOLDERS_H

#include <filesystem>

[[nodiscard]] std::filesystem::path RoamingAppDataFolder();

[[nodiscard]] std::filesystem::path LocalAppDataFolder();

[[nodiscard]] std::filesystem::path SettingsFilePath();

#endif // FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_WINDOWS_KNOWN_FOLDERS_H
