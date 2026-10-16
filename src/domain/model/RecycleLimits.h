#ifndef FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_LIMITS_H
#define FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_LIMITS_H

#include <cstddef>
#include <filesystem>
#include <optional>

inline constexpr std::size_t kTheWindowsRecycleBinStopsAt = 260;

inline constexpr std::size_t kAddonsRoutinelyNest = 200;

inline constexpr std::size_t kTheSeparatorBeforeTheAddon = 1;

[[nodiscard]] constexpr bool TheRecycleBinReaches(const std::optional<std::size_t>& longestEntry,
                                                  const std::optional<std::size_t>& ceiling)
{
    return !ceiling.has_value() || (longestEntry.has_value() && *longestEntry < *ceiling);
}

struct RootDepth
{
    std::size_t characters{};
    std::size_t leavesForTheAddon{};
    std::size_t ceiling{};

    [[nodiscard]] bool ItLeavesRoom() const
    {
        return leavesForTheAddon >= kAddonsRoutinelyNest;
    }
};

[[nodiscard]] inline RootDepth MeasureTheRoot(const std::filesystem::path& root, const std::size_t ceiling)
{
    const std::size_t characters = root.native().size() + kTheSeparatorBeforeTheAddon;

    return {.characters = characters,
            .leavesForTheAddon = characters >= ceiling ? 0 : ceiling - characters,
            .ceiling = ceiling};
}

#endif // FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_LIMITS_H
