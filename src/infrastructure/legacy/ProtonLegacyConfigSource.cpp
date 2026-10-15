#include "infrastructure/legacy/ProtonLegacyConfigSource.h"

std::vector<FoundLegacyInstallation> ProtonLegacyConfigSource::Installations() const
{
    return {};
}

std::vector<LegacyPresetSelection> ProtonLegacyConfigSource::PresetsIn(const std::filesystem::path&) const
{
    return {};
}
