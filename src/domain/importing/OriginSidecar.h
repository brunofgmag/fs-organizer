#ifndef FS_ORGANIZER_DOMAIN_IMPORTING_ORIGIN_SIDECAR_H
#define FS_ORGANIZER_DOMAIN_IMPORTING_ORIGIN_SIDECAR_H

#include <filesystem>
#include <optional>
#include <string>

#include "domain/model/QuarantineOrigin.h"
#include "domain/support/PathUtils.h"

inline constexpr auto kOriginSidecarSuffix = ".fsorg-origin";

[[nodiscard]] std::filesystem::path SidecarPathFor(const std::filesystem::path& item);

[[nodiscard]] std::string TextOfTheOrigin(const QuarantineOrigin& origin);

[[nodiscard]] std::optional<QuarantineOrigin> OriginFromText(const std::string& text);

#endif // FS_ORGANIZER_DOMAIN_IMPORTING_ORIGIN_SIDECAR_H
