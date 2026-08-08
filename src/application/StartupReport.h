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

enum class StartupAlarm : int
{
    None = 0,
    TheExecutableIsMissing = 1,
    TheAddonHoldingItIsOff = 2,
};

struct StartupLine
{
    std::string label{};
    std::filesystem::path path{};
    bool enabled = false;
    StartupReach reach = StartupReach::OutsideYourAddons;
    StartupAlarm alarm = StartupAlarm::None;
    std::filesystem::path addonFolder{};
};

struct StartupReport
{
    std::vector<StartupLine> lines{};
};

[[nodiscard]] StartupReport ReportStartupEntries(const std::vector<StartupEntry>& entries,
                                                 const SimulatorProfile& profile,
                                                 const ProfileSnapshot& snapshot,
                                                 const FilesystemProbe& filesystemProbe);

#endif // FS_ORGANIZER_APPLICATION_STARTUP_REPORT_H
