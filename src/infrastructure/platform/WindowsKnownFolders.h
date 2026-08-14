#ifndef FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_WINDOWS_KNOWN_FOLDERS_H
#define FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_WINDOWS_KNOWN_FOLDERS_H

#include <filesystem>

[[nodiscard]] std::filesystem::path RoamingAppDataFolder();

[[nodiscard]] std::filesystem::path LocalAppDataFolder();

[[nodiscard]] std::filesystem::path ProgramDataFolder();

[[nodiscard]] std::filesystem::path SettingsFilePath();

[[nodiscard]] std::filesystem::path JournalFilePath();

[[nodiscard]] std::filesystem::path PresetsFolderPath();

[[nodiscard]] std::filesystem::path BisectionFolderPath();

[[nodiscard]] std::filesystem::path SceneryCacheFilePath();

[[nodiscard]] std::filesystem::path DocumentIndexFilePath();

#endif // FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_WINDOWS_KNOWN_FOLDERS_H
