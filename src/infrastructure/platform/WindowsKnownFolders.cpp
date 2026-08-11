#include "infrastructure/platform/WindowsKnownFolders.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shlobj.h>

namespace
{
    constexpr auto kApplicationFolderName = "fs-organizer";
    constexpr auto kSettingsFileName = "settings.json";
    constexpr auto kJournalFolderName = "journal";
    constexpr auto kJournalFileName = "operations.jsonl";
    constexpr auto kPresetsFolderName = "presets";
    constexpr auto kSceneryCacheFileName = "scenery-cache.json";

    std::filesystem::path KnownFolder(const KNOWNFOLDERID& folderId)
    {
        PWSTR raw = nullptr;
        if (SHGetKnownFolderPath(folderId, 0, nullptr, &raw) != S_OK)
        {
            CoTaskMemFree(raw);
            return {};
        }

        std::filesystem::path folder(raw);
        CoTaskMemFree(raw);

        return folder;
    }
}

std::filesystem::path RoamingAppDataFolder()
{
    return KnownFolder(FOLDERID_RoamingAppData);
}

std::filesystem::path LocalAppDataFolder()
{
    return KnownFolder(FOLDERID_LocalAppData);
}

std::filesystem::path ProgramDataFolder()
{
    return KnownFolder(FOLDERID_ProgramData);
}

std::filesystem::path SettingsFilePath()
{
    return LocalAppDataFolder() / kApplicationFolderName / kSettingsFileName;
}

std::filesystem::path JournalFilePath()
{
    return LocalAppDataFolder() / kApplicationFolderName / kJournalFolderName / kJournalFileName;
}

std::filesystem::path PresetsFolderPath()
{
    return LocalAppDataFolder() / kApplicationFolderName / kPresetsFolderName;
}

std::filesystem::path SceneryCacheFilePath()
{
    return LocalAppDataFolder() / kApplicationFolderName / kSceneryCacheFileName;
}
