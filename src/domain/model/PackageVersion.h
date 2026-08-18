#ifndef FS_ORGANIZER_DOMAIN_MODEL_PACKAGE_VERSION_H
#define FS_ORGANIZER_DOMAIN_MODEL_PACKAGE_VERSION_H

#include <string>

enum class VersionOrder : int
{
    Older = 0,
    TheSame = 1,
    Newer = 2,
    NoOneCanTell = 3,
};

[[nodiscard]] VersionOrder HowTheVersionCompares(const std::string& version, const std::string& against);

[[nodiscard]] bool TakingItBackIsWorthOffering(const std::string& atTheDestination, const std::string& inTheLibrary);

#endif // FS_ORGANIZER_DOMAIN_MODEL_PACKAGE_VERSION_H
