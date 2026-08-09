#ifndef FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_LIMITS_H
#define FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_LIMITS_H

#include <cstddef>
#include <filesystem>
#include <optional>

inline constexpr std::size_t kTheRecycleBinStopsAt = 260;

inline constexpr std::size_t kAddonsRoutinelyNest = 200;

inline constexpr std::size_t kTheSeparatorBeforeTheAddon = 1;

[[nodiscard]] constexpr bool TheRecycleBinReaches(const std::optional<std::size_t>& longestEntry)
{
    return longestEntry.has_value() && *longestEntry < kTheRecycleBinStopsAt;
}

struct RootDepth
{
    std::size_t characters = 0;
    std::size_t leavesForTheAddon = 0;

    [[nodiscard]] bool ItLeavesRoom() const
    {
        return leavesForTheAddon >= kAddonsRoutinelyNest;
    }
};

[[nodiscard]] inline RootDepth MeasureTheRoot(const std::filesystem::path& root)
{
    const std::size_t characters = root.native().size() + kTheSeparatorBeforeTheAddon;

    return {.characters = characters,
            .leavesForTheAddon = characters >= kTheRecycleBinStopsAt ? 0 : kTheRecycleBinStopsAt - characters};
}

#endif // FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_LIMITS_H
