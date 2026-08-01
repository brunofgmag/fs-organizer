#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_LEGACY_CONFIG_SOURCE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_LEGACY_CONFIG_SOURCE_H

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "application/ports/LegacyConfigSource.h"
#include "domain/support/PathUtils.h"

class FakeLegacyConfigSource final : public LegacyConfigSource
{
public:
    void Add(LegacyInstallation installation)
    {
        const std::filesystem::path folder = installation.folder;

        installations_.push_back(FoundLegacyInstallation{folder, std::move(installation)});
    }

    void AddWithUnreadableConfiguration(const std::filesystem::path& folder)
    {
        installations_.push_back(FoundLegacyInstallation{folder, std::nullopt});
    }

    void PlacePreset(const std::filesystem::path& folder, LegacyPresetSelection preset)
    {
        presets_[ComparablePath(folder)].push_back(std::move(preset));
    }

    [[nodiscard]] std::vector<FoundLegacyInstallation> Installations() const override
    {
        return installations_;
    }

    [[nodiscard]] std::vector<LegacyPresetSelection> PresetsIn(const std::filesystem::path& folder) const override
    {
        const auto placed = presets_.find(ComparablePath(folder));

        return placed == presets_.end() ? std::vector<LegacyPresetSelection>{} : placed->second;
    }

private:
    std::vector<FoundLegacyInstallation> installations_;
    std::map<std::string, std::vector<LegacyPresetSelection>> presets_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_LEGACY_CONFIG_SOURCE_H
