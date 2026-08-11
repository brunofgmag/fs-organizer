#include "application/SceneryService.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "domain/model/SceneryFolder.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    constexpr std::size_t kEnoughForTheSectionTable = 64 * 1024;
    constexpr auto kSceneryFileSuffix = ".bgl";

    [[nodiscard]] bool ItIsASceneryFile(const std::filesystem::path& file)
    {
        return ComparableFileName(file).ends_with(kSceneryFileSuffix);
    }

    [[nodiscard]] std::span<const std::uint8_t> AsBytes(const std::string& text)
    {
        return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
    }
}

SceneryService::SceneryService(const FilesystemProbe& filesystemProbe,
                               const SceneryParser& parser,
                               const Clock& clock,
                               SceneryCache& cache)
    : filesystemProbe_(filesystemProbe), parser_(parser), clock_(clock), cache_(cache)
{
}

std::vector<std::filesystem::path> SceneryService::SceneryFoldersOf(const std::filesystem::path& addonFolder) const
{
    std::vector<std::filesystem::path> folders;

    for (const std::filesystem::path& child : filesystemProbe_.ChildDirectories(addonFolder))
    {
        if (ItIsTheSceneryFolderOfAnAddon(child))
        {
            folders.push_back(child);
        }
    }

    for (std::size_t at = 0; at < folders.size(); ++at)
    {
        for (const std::filesystem::path& child : filesystemProbe_.ChildDirectories(folders[at]))
        {
            folders.push_back(child);
        }
    }

    return folders;
}

std::vector<std::filesystem::path> SceneryService::SceneryFilesOf(const std::filesystem::path& addonFolder) const
{
    std::vector<std::filesystem::path> files;

    for (const std::filesystem::path& folder : SceneryFoldersOf(addonFolder))
    {
        for (const std::filesystem::path& file : filesystemProbe_.ChildFiles(folder))
        {
            if (ItIsASceneryFile(file))
            {
                files.push_back(file);
            }
        }
    }

    return files;
}

std::vector<SceneryCodes> SceneryService::ReadTheFilesOf(const std::filesystem::path& addonFolder) const
{
    std::vector<SceneryCodes> read;

    for (const std::filesystem::path& file : SceneryFilesOf(addonFolder))
    {
        const std::optional<std::string> head = filesystemProbe_.FirstBytesOf(file, kEnoughForTheSectionTable);
        if (!head.has_value() || !parser_.CouldCarryAnAirportSection(AsBytes(*head)))
        {
            continue;
        }

        const std::optional<std::string> whole = filesystemProbe_.ContentsOf(file);
        if (!whole.has_value())
        {
            read.push_back({.reading = SceneryReading::ItEndsBeforeItSaysItDoes});
            continue;
        }

        read.push_back(parser_.Parse(AsBytes(*whole)));
    }

    return read;
}

std::optional<std::chrono::system_clock::time_point>
SceneryService::WhenTheSceneryLastChanged(const std::filesystem::path& addonFolder) const
{
    std::optional<std::chrono::system_clock::time_point> newest = filesystemProbe_.LastWriteTime(addonFolder);

    for (const std::filesystem::path& folder : SceneryFoldersOf(addonFolder))
    {
        const std::optional<std::chrono::system_clock::time_point> changed = filesystemProbe_.LastWriteTime(folder);

        if (changed.has_value())
        {
            newest = newest.has_value() ? std::max(*newest, *changed) : changed;
        }
    }

    return newest;
}

std::vector<AddonToRead> SceneryService::AddonsOf(const SimulatorProfile& profile, const ProfileSnapshot& snapshot)
{
    std::vector<AddonToRead> addons;

    for (const TreeNode& library : snapshot.libraries)
    {
        for (const TreeNode* addon : AddonsUnder(library))
        {
            addons.push_back({.addon = IdentityOf(profile, addon->path), .folder = addon->path});
        }
    }

    return addons;
}

std::vector<SceneryOfAnAddon> SceneryService::WhatIsAlreadyKnown(const std::vector<AddonToRead>& addons) const
{
    std::vector<SceneryOfAnAddon> known;

    for (const AddonToRead& addon : addons)
    {
        const std::optional<RememberedScenery> remembered = cache_.Remember(addon.folder);

        if (remembered.has_value())
        {
            known.push_back({.addon = addon.addon, .resolvedPath = addon.folder, .files = remembered->files});
        }
    }

    return known;
}

SceneryOfAnAddon SceneryService::SceneryOf(const AddonToRead& addon, const SceneryFreshness freshness)
{
    const std::optional<RememberedScenery> remembered =
        freshness == SceneryFreshness::ReadAgain ? std::nullopt : cache_.Remember(addon.folder);
    const std::optional<std::chrono::system_clock::time_point> changed = WhenTheSceneryLastChanged(addon.folder);

    if (remembered.has_value() && changed.has_value() && *changed <= remembered->readAt)
    {
        return {.addon = addon.addon, .resolvedPath = addon.folder, .files = remembered->files};
    }

    const RememberedScenery read{.readAt = clock_.Now(), .files = ReadTheFilesOf(addon.folder)};
    cache_.Keep(addon.folder, read);

    return {.addon = addon.addon, .resolvedPath = addon.folder, .files = read.files};
}

std::vector<SceneryOfAnAddon> SceneryService::SceneryOfEach(const std::vector<AddonToRead>& addons,
                                                            const SceneryProgress& onProgress,
                                                            const SceneryFreshness freshness)
{
    std::vector<SceneryOfAnAddon> scenery;
    scenery.reserve(addons.size());

    for (const AddonToRead& addon : addons)
    {
        scenery.push_back(SceneryOf(addon, freshness));

        if (onProgress && !onProgress(scenery.size(), addons.size()))
        {
            break;
        }
    }

    return scenery;
}
