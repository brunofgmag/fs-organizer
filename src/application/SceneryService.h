#ifndef FS_ORGANIZER_APPLICATION_SCENERY_SERVICE_H
#define FS_ORGANIZER_APPLICATION_SCENERY_SERVICE_H

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

#include "application/model/ProfileSnapshot.h"
#include "application/ports/SceneryCache.h"
#include "domain/model/AddonId.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/ports/Clock.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/SceneryParser.h"
#include "domain/scenery/AirportCoverage.h"

struct AddonToRead
{
    AddonId addon{};
    std::filesystem::path folder{};
    bool itIsNavigationData = false;
};

using SceneryProgress = std::function<bool(std::size_t read, std::size_t outOf)>;

enum class SceneryFreshness : int
{
    ReuseWhatIsKnown = 0,
    ReadAgain = 1,
};

class SceneryService
{
public:
    SceneryService(const FilesystemProbe& filesystemProbe,
                   const SceneryParser& parser,
                   const Clock& clock,
                   SceneryCache& cache);

    [[nodiscard]] static std::vector<AddonToRead> AddonsOf(const SimulatorProfile& profile,
                                                           const ProfileSnapshot& snapshot);

    [[nodiscard]] SceneryOfAnAddon SceneryOf(const AddonToRead& addon,
                                             SceneryFreshness freshness = SceneryFreshness::ReuseWhatIsKnown);

    [[nodiscard]] std::vector<SceneryOfAnAddon>
    SceneryOfEach(const std::vector<AddonToRead>& addons,
                  const SceneryProgress& onProgress,
                  SceneryFreshness freshness = SceneryFreshness::ReuseWhatIsKnown);

    [[nodiscard]] std::vector<SceneryOfAnAddon> WhatIsAlreadyKnown(const std::vector<AddonToRead>& addons) const;

private:
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point>
    WhenTheSceneryLastChanged(const std::filesystem::path& addonFolder) const;

    [[nodiscard]] std::vector<std::filesystem::path> SceneryFoldersOf(const std::filesystem::path& addonFolder) const;

    [[nodiscard]] std::vector<std::filesystem::path> SceneryFilesOf(const std::filesystem::path& addonFolder) const;

    [[nodiscard]] std::vector<SceneryCodes> ReadTheFilesOf(const std::filesystem::path& addonFolder) const;

    const FilesystemProbe& filesystemProbe_;
    const SceneryParser& parser_;
    const Clock& clock_;
    SceneryCache& cache_;
};

#endif // FS_ORGANIZER_APPLICATION_SCENERY_SERVICE_H
