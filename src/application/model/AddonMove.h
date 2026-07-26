#ifndef FS_ORGANIZER_APPLICATION_MODEL_ADDON_MOVE_H
#define FS_ORGANIZER_APPLICATION_MODEL_ADDON_MOVE_H

#include <filesystem>

struct AddonMove
{
    std::filesystem::path addonFolder;
    std::filesystem::path category;

    [[nodiscard]] std::filesystem::path Target() const
    {
        return category / addonFolder.filename();
    }
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_ADDON_MOVE_H
