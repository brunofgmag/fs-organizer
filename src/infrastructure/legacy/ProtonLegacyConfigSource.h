#ifndef FS_ORGANIZER_INFRASTRUCTURE_LEGACY_PROTON_LEGACY_CONFIG_SOURCE_H
#define FS_ORGANIZER_INFRASTRUCTURE_LEGACY_PROTON_LEGACY_CONFIG_SOURCE_H

#include <filesystem>
#include <vector>

#include "application/ports/LegacyConfigSource.h"
#include "infrastructure/sim/ProtonPrefix.h"

class ProtonLegacyConfigSource final : public LegacyConfigSource
{
public:
    explicit ProtonLegacyConfigSource(std::vector<ProtonPrefix> prefixes);

    [[nodiscard]] std::vector<FoundLegacyInstallation> Installations() const override;

    [[nodiscard]] std::vector<LegacyPresetSelection> PresetsIn(const std::filesystem::path& folder) const override;

private:
    std::vector<ProtonPrefix> prefixes_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_LEGACY_PROTON_LEGACY_CONFIG_SOURCE_H
