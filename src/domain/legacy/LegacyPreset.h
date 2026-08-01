#ifndef FS_ORGANIZER_DOMAIN_LEGACY_LEGACY_PRESET_H
#define FS_ORGANIZER_DOMAIN_LEGACY_LEGACY_PRESET_H

#include <filesystem>
#include <string>
#include <vector>

#include "domain/legacy/LegacyPresetSelection.h"
#include "domain/model/Preset.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/model/TreeNode.h"

struct ImportedPreset
{
    Preset preset;
    std::vector<std::string> unresolvedAddonNames;
    std::vector<std::filesystem::path> unresolvedFolders;
};

[[nodiscard]] ImportedPreset ImportLegacyPreset(const LegacyPresetSelection& selection,
                                                const SimulatorProfile& profile,
                                                const std::vector<TreeNode>& libraries);

ImportedPreset ImportLegacyPreset(const LegacyPresetSelection& selection,
                                  const SimulatorProfile& profile,
                                  std::vector<TreeNode>&& libraries) = delete;

#endif // FS_ORGANIZER_DOMAIN_LEGACY_LEGACY_PRESET_H
