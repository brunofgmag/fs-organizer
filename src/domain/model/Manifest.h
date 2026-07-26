#ifndef FS_ORGANIZER_DOMAIN_MODEL_MANIFEST_H
#define FS_ORGANIZER_DOMAIN_MODEL_MANIFEST_H

#include <filesystem>
#include <string>

inline constexpr auto kManifestFileName = "manifest.json";

[[nodiscard]] inline std::filesystem::path ManifestPathIn(const std::filesystem::path& folder)
{
    return folder / kManifestFileName;
}

struct Manifest
{
    std::string title;
    std::string creator;
    std::string manufacturer;
    std::string contentType;
    std::string packageVersion;
    std::string minimumGameVersion;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_MANIFEST_H
