#ifndef FS_ORGANIZER_APPLICATION_MODEL_ADDON_MOVE_H
#define FS_ORGANIZER_APPLICATION_MODEL_ADDON_MOVE_H

#include <filesystem>

#include "domain/model/LandingPath.h"

struct AddonMove
{
    std::filesystem::path addonFolder;
    std::filesystem::path category;

    [[nodiscard]] std::filesystem::path Target() const
    {
        return LandingPathIn(category, addonFolder);
    }
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_ADDON_MOVE_H
