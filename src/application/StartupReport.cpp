#include "application/StartupReport.h"

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"

namespace
{
    std::filesystem::path FolderReachedIn(const std::filesystem::path& destination,
                                          const std::filesystem::path& entryPath)
    {
        const std::string wanted = ComparablePath(destination);

        std::filesystem::path folder = entryPath.parent_path();
        while (!folder.empty() && ComparablePath(folder.parent_path()) != wanted)
        {
            if (folder.parent_path() == folder)
            {
                return {};
            }

            folder = folder.parent_path();
        }

        return folder;
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
        const TreeNode* addon = AddonNamed(snapshot.libraries, AsUtf8(folder.filename()));

        return addon != nullptr && !snapshot.enabled.Contains(addon->path);
    }

    StartupAlarm AlarmFor(const StartupEntry& entry,
                          const std::filesystem::path& addonFolder,
                          const ProfileSnapshot& snapshot,
                          const FilesystemProbe& filesystemProbe)
    {
        if (!entry.enabled || filesystemProbe.EntryExistsWithoutFollowingLinks(entry.path))
        {
            return StartupAlarm::None;
        }

        if (!addonFolder.empty() && AnAddonOfYoursLandsThereAndIsOffNow(snapshot, addonFolder))
        {
            return StartupAlarm::TheAddonHoldingItIsOff;
        }

        return StartupAlarm::TheExecutableIsMissing;
    }
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

        report.lines.push_back(
            StartupLine{.label = entry.label,
                        .path = entry.path,
                        .enabled = entry.enabled,
                        .reach = addonFolder.empty() ? StartupReach::OutsideYourAddons : StartupReach::InsideAnAddon,
                        .alarm = AlarmFor(entry, addonFolder, snapshot, filesystemProbe),
                        .addonFolder = addonFolder});
    }

    return report;
}
