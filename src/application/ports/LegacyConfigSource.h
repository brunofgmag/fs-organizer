#ifndef FS_ORGANIZER_APPLICATION_PORTS_LEGACY_CONFIG_SOURCE_H
#define FS_ORGANIZER_APPLICATION_PORTS_LEGACY_CONFIG_SOURCE_H

#include <filesystem>
#include <vector>

#include "application/model/FoundLegacyInstallation.h"
#include "domain/legacy/LegacyPresetSelection.h"

class LegacyConfigSource
{
public:
    virtual ~LegacyConfigSource() = default;

    [[nodiscard]] virtual std::vector<FoundLegacyInstallation> Installations() const = 0;

    [[nodiscard]] virtual std::vector<LegacyPresetSelection> PresetsIn(const std::filesystem::path& folder) const = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_LEGACY_CONFIG_SOURCE_H
