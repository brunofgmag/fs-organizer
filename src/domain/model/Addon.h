#ifndef FS_ORGANIZER_DOMAIN_MODEL_ADDON_H
#define FS_ORGANIZER_DOMAIN_MODEL_ADDON_H

#include <filesystem>

#include "domain/model/Manifest.h"

struct Addon
{
    std::filesystem::path folderPath;
    Manifest manifest{};
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_ADDON_H
