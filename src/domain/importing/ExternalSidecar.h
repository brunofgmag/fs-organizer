#ifndef FS_ORGANIZER_DOMAIN_IMPORTING_EXTERNAL_SIDECAR_H
#define FS_ORGANIZER_DOMAIN_IMPORTING_EXTERNAL_SIDECAR_H

#include <filesystem>
#include <optional>
#include <string>

#include "domain/support/PathUtils.h"

inline constexpr auto kExternalSidecarSuffix = ".fsorg-external";

[[nodiscard]] std::filesystem::path ExternalSidecarPathFor(const std::filesystem::path& addonFolder);

[[nodiscard]] std::string TextOfTheExternalOrigin(const std::filesystem::path& externalPath);

[[nodiscard]] std::optional<std::filesystem::path> ExternalOriginFromText(const std::string& text);

#endif // FS_ORGANIZER_DOMAIN_IMPORTING_EXTERNAL_SIDECAR_H
