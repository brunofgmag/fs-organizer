#ifndef FS_ORGANIZER_APPLICATION_MODEL_FOUND_LEGACY_INSTALLATION_H
#define FS_ORGANIZER_APPLICATION_MODEL_FOUND_LEGACY_INSTALLATION_H

#include <filesystem>
#include <optional>

#include "domain/legacy/LegacyInstallation.h"

struct FoundLegacyInstallation
{
    std::filesystem::path folder;
    std::optional<LegacyInstallation> configuration;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_FOUND_LEGACY_INSTALLATION_H
