#ifndef FS_ORGANIZER_DOMAIN_MODEL_CATEGORY_MARKER_H
#define FS_ORGANIZER_DOMAIN_MODEL_CATEGORY_MARKER_H

#include <filesystem>

inline constexpr auto kCategoryMarkerName = ".fsorg-category";

[[nodiscard]] inline std::filesystem::path CategoryMarkerPathIn(const std::filesystem::path& folder)
{
    return folder / kCategoryMarkerName;
}

#endif // FS_ORGANIZER_DOMAIN_MODEL_CATEGORY_MARKER_H
