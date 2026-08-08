#ifndef FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_LIMITS_H
#define FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_LIMITS_H

#include <cstddef>
#include <optional>

inline constexpr std::size_t kTheRecycleBinStopsAt = 260;

[[nodiscard]] constexpr bool TheRecycleBinReaches(const std::optional<std::size_t>& longestEntry)
{
    return longestEntry.has_value() && *longestEntry < kTheRecycleBinStopsAt;
}

#endif // FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_LIMITS_H
