#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_STEAM_LIBRARY_FOLDERS_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_STEAM_LIBRARY_FOLDERS_H

#include <filesystem>
#include <vector>

[[nodiscard]] std::vector<std::filesystem::path> SteamLibraryFoldersIn(const std::filesystem::path& file);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_STEAM_LIBRARY_FOLDERS_H
