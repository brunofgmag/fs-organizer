#ifndef FS_ORGANIZER_DOMAIN_MODEL_SCENERY_FOLDER_H
#define FS_ORGANIZER_DOMAIN_MODEL_SCENERY_FOLDER_H

#include <filesystem>

#include "domain/support/PathUtils.h"

inline constexpr auto kSceneryFolderName = "scenery";

[[nodiscard]] inline bool ItIsTheSceneryFolderOfAnAddon(const std::filesystem::path& folder)
{
    return ComparableFileName(folder) == kSceneryFolderName;
}

#endif // FS_ORGANIZER_DOMAIN_MODEL_SCENERY_FOLDER_H
