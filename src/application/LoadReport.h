#ifndef FS_ORGANIZER_APPLICATION_LOAD_REPORT_H
#define FS_ORGANIZER_APPLICATION_LOAD_REPORT_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "application/model/ProfileSnapshot.h"
#include "application/ports/LoadingReportSource.h"

struct ModuleLine
{
    std::string moduleName{};
    std::string packageName{};
    std::optional<std::uintmax_t> memoryBytes{};
    std::filesystem::path addonUnderLibrary{};
};

struct LoadDiagnostics
{
    std::vector<ModuleLine> modules{};
    std::size_t packagesRegistered = 0;
    std::optional<std::chrono::system_clock::time_point> runAt{};
    bool reportWasRead = false;
};

[[nodiscard]] LoadDiagnostics ReportTheLoad(const std::optional<LoadingReport>& report,
                                            const ProfileSnapshot& snapshot);

#endif // FS_ORGANIZER_APPLICATION_LOAD_REPORT_H
