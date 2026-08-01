#ifndef FS_ORGANIZER_DOMAIN_LEGACY_LEGACY_PRESET_SELECTION_H
#define FS_ORGANIZER_DOMAIN_LEGACY_LEGACY_PRESET_SELECTION_H

#include <filesystem>
#include <string>
#include <vector>

struct LegacyPresetSelection
{
    std::string name;
    std::vector<std::string> enabledAddonNames;
    std::vector<std::filesystem::path> enabledFolders;
    std::vector<std::string> disabledAddonNames;
};

#endif // FS_ORGANIZER_DOMAIN_LEGACY_LEGACY_PRESET_SELECTION_H
