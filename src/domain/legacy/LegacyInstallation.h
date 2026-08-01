#ifndef FS_ORGANIZER_DOMAIN_LEGACY_LEGACY_INSTALLATION_H
#define FS_ORGANIZER_DOMAIN_LEGACY_LEGACY_INSTALLATION_H

#include <filesystem>
#include <string>
#include <vector>

struct LegacyInstallation
{
    std::filesystem::path folder;
    std::vector<std::filesystem::path> addonPaths;
    std::filesystem::path communityPath;
    std::filesystem::path presetsPath;
    std::string linkType;
};

#endif // FS_ORGANIZER_DOMAIN_LEGACY_LEGACY_INSTALLATION_H
