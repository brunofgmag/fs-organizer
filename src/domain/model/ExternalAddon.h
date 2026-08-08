#ifndef FS_ORGANIZER_DOMAIN_MODEL_EXTERNAL_ADDON_H
#define FS_ORGANIZER_DOMAIN_MODEL_EXTERNAL_ADDON_H

#include <filesystem>

struct ExternalAddon
{
    std::filesystem::path addonFolder{};
    std::filesystem::path externalPath{};
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_EXTERNAL_ADDON_H
