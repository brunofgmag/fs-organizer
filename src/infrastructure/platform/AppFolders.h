#ifndef FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_APP_FOLDERS_H
#define FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_APP_FOLDERS_H

#include <filesystem>

struct AppFolders
{
    std::filesystem::path settingsFile{};
    std::filesystem::path journalFile{};
    std::filesystem::path presetsFolder{};
    std::filesystem::path removedStartupEntriesFolder{};
    std::filesystem::path bisectionFolder{};
    std::filesystem::path sceneryCacheFile{};
    std::filesystem::path documentIndexFile{};
    std::filesystem::path manualFolder{};
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_APP_FOLDERS_H
