#include "application/StartupReport.h"

#include <set>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"

namespace
{
    std::filesystem::path FolderReachedIn(const std::filesystem::path& destination,
                                          const std::filesystem::path& entryPath)
    {
        const std::string wanted = ComparablePath(destination);

        std::filesystem::path folder = ParentOf(entryPath);
        while (!folder.empty())
        {
            const std::filesystem::path above = ParentOf(folder);
            if (ComparablePath(above) == wanted)
            {
                return folder;
            }

            folder = above;
        }

        return {};
    }

    std::filesystem::path AddonFolderReachedBy(const SimulatorProfile& profile, const std::filesystem::path& entryPath)
    {
        for (const std::filesystem::path& destination : profile.destinations)
        {
            if (!PathIsInside(entryPath, destination))
            {
                continue;
            }

            const std::filesystem::path folder = FolderReachedIn(destination, entryPath);
            if (!folder.empty())
            {
                return folder;
            }
        }

        return {};
    }

    bool AnAddonOfYoursLandsThereAndIsOffNow(const ProfileSnapshot& snapshot, const std::filesystem::path& folder)
    {
        const TreeNode* addon = AddonNamed(snapshot.libraries, AsUtf8(folder));

        return addon != nullptr && !snapshot.enabled.Contains(addon->path);
    }

    struct OnDisk
    {
        bool exists = false;
        bool addonIsOff = false;
    };

    OnDisk ReadOnDisk(const StartupEntry& entry,
                      const std::filesystem::path& addonFolder,
                      const ProfileSnapshot& snapshot,
                      const FilesystemProbe& filesystemProbe)
    {
        if (filesystemProbe.EntryExistsWithoutFollowingLinks(entry.path))
        {
            return {.exists = true, .addonIsOff = false};
        }

        return {.exists = false,
                .addonIsOff = !addonFolder.empty() && AnAddonOfYoursLandsThereAndIsOffNow(snapshot, addonFolder)};
    }

    StartupCondition
    ConditionOf(const std::filesystem::path& entryPath, const OnDisk& onDisk, const FilesystemProbe& filesystemProbe)
    {
        if (onDisk.exists)
        {
            return StartupCondition::Reachable;
        }

        if (onDisk.addonIsOff)
        {
            return StartupCondition::BehindADisabledAddon;
        }

        return filesystemProbe.VolumeIsAvailable(entryPath) ? StartupCondition::Broken : StartupCondition::Unavailable;
    }
}

std::vector<StartupLine> EntriesCarriedBy(const StartupReport& report,
                                          const std::vector<std::filesystem::path>& addonFolders)
{
    std::set<std::string> wanted;
    for (const std::filesystem::path& folder : addonFolders)
    {
        wanted.insert(ComparableFileName(folder));
    }

    std::vector<StartupLine> carried;
    for (const StartupLine& line : report.lines)
    {
        if (!line.enabled || line.addonFolder.empty())
        {
            continue;
        }

        if (wanted.contains(ComparableFileName(line.addonFolder)))
        {
            carried.push_back(line);
        }
    }

    return carried;
}

StartupReport ReportStartupEntries(const std::vector<StartupEntry>& entries,
                                   const SimulatorProfile& profile,
                                   const ProfileSnapshot& snapshot,
                                   const FilesystemProbe& filesystemProbe)
{
    StartupReport report;

    for (const StartupEntry& entry : entries)
    {
        const std::filesystem::path addonFolder = AddonFolderReachedBy(profile, entry.path);
        const OnDisk onDisk = ReadOnDisk(entry, addonFolder, snapshot, filesystemProbe);

        report.lines.push_back(
            StartupLine{.label = entry.label,
                        .path = entry.path,
                        .enabled = entry.enabled,
                        .reach = addonFolder.empty() ? StartupReach::OutsideYourAddons : StartupReach::InsideAnAddon,
                        .addonFolder = addonFolder,
                        .condition = ConditionOf(entry.path, onDisk, filesystemProbe),
                        .commandLine = entry.commandLine});
    }

    return report;
}
