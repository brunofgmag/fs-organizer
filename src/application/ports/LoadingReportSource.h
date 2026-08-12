#ifndef FS_ORGANIZER_APPLICATION_PORTS_LOADING_REPORT_SOURCE_H
#define FS_ORGANIZER_APPLICATION_PORTS_LOADING_REPORT_SOURCE_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct LoadedModule
{
    std::string moduleName{};
    std::string packageName{};
    std::string packageFolderName{};
    std::optional<std::uintmax_t> memoryBytes{};
};

struct LoadingReport
{
    std::vector<LoadedModule> modules{};
    std::size_t packagesRegistered = 0;
    std::optional<std::chrono::system_clock::time_point> runAt{};
};

class LoadingReportSource
{
public:
    virtual ~LoadingReportSource() = default;

    [[nodiscard]] virtual std::optional<LoadingReport> LastReport() const = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_LOADING_REPORT_SOURCE_H
