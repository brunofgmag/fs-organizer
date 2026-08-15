#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_LOADING_REPORT_SOURCE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_LOADING_REPORT_SOURCE_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "application/ports/LoadingReportSource.h"

class FakeLoadingReportSource final : public LoadingReportSource
{
public:
    void ReportAModule(std::string moduleName, std::string packageName, const std::uintmax_t memoryBytes)
    {
        Reported().modules.push_back({.moduleName = std::move(moduleName),
                                      .packageName = packageName,
                                      .packageFolderName = std::move(packageName),
                                      .memoryBytes = memoryBytes});
    }

    void ReportPackagesRegistered(const std::size_t howMany)
    {
        Reported().packagesRegistered = howMany;
    }

    [[nodiscard]] std::optional<LoadingReport> LastReport() const override
    {
        return report_;
    }

private:
    [[nodiscard]] LoadingReport& Reported()
    {
        if (!report_.has_value())
        {
            report_.emplace();
        }

        return *report_;
    }

    std::optional<LoadingReport> report_{};
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_LOADING_REPORT_SOURCE_H
