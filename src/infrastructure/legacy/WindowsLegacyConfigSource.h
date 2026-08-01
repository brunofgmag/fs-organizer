#ifndef FS_ORGANIZER_INFRASTRUCTURE_LEGACY_WINDOWS_LEGACY_CONFIG_SOURCE_H
#define FS_ORGANIZER_INFRASTRUCTURE_LEGACY_WINDOWS_LEGACY_CONFIG_SOURCE_H

#include <filesystem>
#include <vector>

#include "application/ports/LegacyConfigSource.h"

class WindowsLegacyConfigSource final : public LegacyConfigSource
{
public:
    explicit WindowsLegacyConfigSource(std::filesystem::path programData);

    [[nodiscard]] std::vector<FoundLegacyInstallation> Installations() const override;

    [[nodiscard]] std::vector<LegacyPresetSelection> PresetsIn(const std::filesystem::path& folder) const override;

private:
    std::filesystem::path programData_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_LEGACY_WINDOWS_LEGACY_CONFIG_SOURCE_H
