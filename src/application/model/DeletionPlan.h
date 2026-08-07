#ifndef FS_ORGANIZER_APPLICATION_MODEL_DELETION_PLAN_H
#define FS_ORGANIZER_APPLICATION_MODEL_DELETION_PLAN_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "domain/model/AddonId.h"
#include "domain/model/FileResult.h"

enum class DeletionRoute : int
{
    RecycleBin = 0,
    Permanently = 1,
};

struct EnabledSomewhere
{
    std::string profileId{};
    std::filesystem::path linkPath{};
};

struct AddonToDelete
{
    std::filesystem::path folder{};
    AddonId addonId{};
    std::vector<EnabledSomewhere> enabled{};
    std::optional<std::uintmax_t> bytes{};
    std::optional<std::size_t> longestEntry{};
};

struct VolumeRoom
{
    std::filesystem::path volume{};
    std::optional<std::uintmax_t> selected{};
    std::optional<std::uintmax_t> quota{};
    bool itRecycles = true;
};

struct DeletionPlan
{
    std::vector<AddonToDelete> addons{};
    std::size_t nodesThatAreNotAddons = 0;
    std::vector<VolumeRoom> volumes{};
};

struct DeletionResult
{
    std::filesystem::path folder{};
    FileResult result = FileResult::Completed;
    std::vector<std::filesystem::path> linksRemoved{};
};

[[nodiscard]] std::filesystem::path VolumeOf(const std::filesystem::path& path);

[[nodiscard]] bool TheVolumeCanTake(const VolumeRoom& room);

[[nodiscard]] const VolumeRoom* VolumeHolding(const DeletionPlan& plan, const std::filesystem::path& folder);

[[nodiscard]] bool TheRecycleBinCanTake(const DeletionPlan& plan);

#endif // FS_ORGANIZER_APPLICATION_MODEL_DELETION_PLAN_H
