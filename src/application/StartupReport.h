#ifndef FS_ORGANIZER_APPLICATION_STARTUP_REPORT_H
#define FS_ORGANIZER_APPLICATION_STARTUP_REPORT_H

#include <filesystem>
#include <string>
#include <vector>

#include "application/model/ProfileSnapshot.h"
#include "application/ports/StartupEntries.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/ports/FilesystemProbe.h"

enum class StartupReach : int
{
    OutsideYourAddons = 0,
    InsideAnAddon = 1,
};

enum class StartupCondition : int
{
    Reachable = 0,
    BehindADisabledAddon = 1,
    Broken = 2,
    Unavailable = 3,
};

struct StartupLine
{
    std::string label{};
    std::filesystem::path path{};
    bool enabled = false;
    StartupReach reach = StartupReach::OutsideYourAddons;
    std::filesystem::path addonFolder{};
    StartupCondition condition = StartupCondition::Reachable;
    std::string commandLine{};
};

struct StartupReport
{
    std::vector<StartupLine> lines{};
};

[[nodiscard]] StartupReport ReportStartupEntries(const std::vector<StartupEntry>& entries,
                                                 const SimulatorProfile& profile,
                                                 const ProfileSnapshot& snapshot,
                                                 const FilesystemProbe& filesystemProbe);

[[nodiscard]] std::vector<StartupLine> EntriesCarriedBy(const StartupReport& report,
                                                        const std::vector<std::filesystem::path>& addonFolders);

#endif // FS_ORGANIZER_APPLICATION_STARTUP_REPORT_H
