#ifndef FS_ORGANIZER_DOMAIN_MODEL_LANDING_PATH_H
#define FS_ORGANIZER_DOMAIN_MODEL_LANDING_PATH_H

#include <filesystem>

[[nodiscard]] inline std::filesystem::path LandingPathIn(const std::filesystem::path& category,
                                                         const std::filesystem::path& folder)
{
    return category / folder.filename();
}

#endif // FS_ORGANIZER_DOMAIN_MODEL_LANDING_PATH_H
