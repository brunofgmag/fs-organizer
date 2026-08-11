#ifndef FS_ORGANIZER_DOMAIN_MODEL_PRESET_H
#define FS_ORGANIZER_DOMAIN_MODEL_PRESET_H

#include <filesystem>
#include <string>
#include <vector>

#include "domain/model/AddonId.h"

enum class PresetAction : int
{
    Enable = 0,
    Disable = 1,
};

struct PresetEntry
{
    AddonId addonId{};
    PresetAction action = PresetAction::Enable;
};

struct PresetStartupEntry
{
    std::filesystem::path path{};
    PresetAction action = PresetAction::Enable;
};

struct Preset
{
    std::string name{};
    std::vector<PresetEntry> entries{};
    std::vector<PresetStartupEntry> startupEntries{};
    bool governsStartup = false;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_PRESET_H
