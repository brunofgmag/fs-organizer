#ifndef FS_ORGANIZER_DOMAIN_MODEL_QUARANTINE_ORIGIN_H
#define FS_ORGANIZER_DOMAIN_MODEL_QUARANTINE_ORIGIN_H

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>

enum class OriginSource : int
{
    Unknown = 0,
    Sidecar = 1,
    Journal = 2,
};

inline constexpr std::array kAllOriginSources{
    OriginSource::Unknown,
    OriginSource::Sidecar,
    OriginSource::Journal,
};

static_assert(kAllOriginSources.size() == static_cast<std::size_t>(OriginSource::Journal) + 1,
              "Every OriginSource belongs in kAllOriginSources, and the last one carries the highest value.");

struct QuarantineOrigin
{
    std::filesystem::path origin{};
    std::optional<std::chrono::system_clock::time_point> quarantinedAt{};
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_QUARANTINE_ORIGIN_H
