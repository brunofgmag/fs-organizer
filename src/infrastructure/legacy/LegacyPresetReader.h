#ifndef FS_ORGANIZER_INFRASTRUCTURE_LEGACY_LEGACY_PRESET_READER_H
#define FS_ORGANIZER_INFRASTRUCTURE_LEGACY_LEGACY_PRESET_READER_H

#include <filesystem>
#include <optional>
#include <vector>

#include "domain/legacy/LegacyPresetSelection.h"

[[nodiscard]] std::optional<LegacyPresetSelection> ReadLegacyPreset(const std::filesystem::path& file);

[[nodiscard]] std::vector<LegacyPresetSelection> ReadLegacyPresetsIn(const std::filesystem::path& folder);

#endif // FS_ORGANIZER_INFRASTRUCTURE_LEGACY_LEGACY_PRESET_READER_H
