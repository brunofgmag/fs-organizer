#ifndef FS_ORGANIZER_VIEWMODEL_MOVE_TARGET_H
#define FS_ORGANIZER_VIEWMODEL_MOVE_TARGET_H

#include <filesystem>

struct MoveTarget
{
    std::filesystem::path category;
    std::filesystem::path relativePath;
};

#endif // FS_ORGANIZER_VIEWMODEL_MOVE_TARGET_H
