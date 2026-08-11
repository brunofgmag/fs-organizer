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
