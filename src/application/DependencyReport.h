#ifndef FS_ORGANIZER_APPLICATION_DEPENDENCY_REPORT_H
#define FS_ORGANIZER_APPLICATION_DEPENDENCY_REPORT_H

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "application/model/ProfileSnapshot.h"
#include "domain/model/Addon.h"
#include "domain/ports/SimulatorPackages.h"

enum class DependencyResolution : int
{
    InThisLibrary = 0,
    InTheSimulator = 1,
    Unverifiable = 2,
};

struct DependencyAnswer
{
    std::string name;
    std::string declaredVersion;
    DependencyResolution resolution = DependencyResolution::Unverifiable;
    bool enabled = false;
    std::string libraryVersion;
};

struct DependencyReport
{
    std::vector<DependencyAnswer> answers;
    std::optional<std::chrono::system_clock::time_point> listTakenAt;
    std::string listAccountFolder;
};

[[nodiscard]] DependencyReport
ReportDependencies(const Addon& addon, const ProfileSnapshot& snapshot, const SimulatorPackages& packages);

#endif // FS_ORGANIZER_APPLICATION_DEPENDENCY_REPORT_H
