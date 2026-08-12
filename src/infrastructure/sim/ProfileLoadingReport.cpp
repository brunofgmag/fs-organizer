#include "infrastructure/sim/ProfileLoadingReport.h"

#include <string>
#include <utility>

#include "infrastructure/sim/LoadingReportText.h"

namespace
{
    [[nodiscard]] bool ItSaidSomething(const LoadingReport& report)
    {
        return report.runAt.has_value() || report.packagesRegistered > 0 || !report.modules.empty();
    }
}

ProfileLoadingReport::ProfileLoadingReport(const FilesystemProbe& filesystemProbe, std::filesystem::path reportPath)
    : filesystemProbe_(filesystemProbe), reportPath_(std::move(reportPath))
{
}

void ProfileLoadingReport::Use(std::filesystem::path reportPath)
{
    reportPath_ = std::move(reportPath);
}

std::optional<LoadingReport> ProfileLoadingReport::LastReport() const
{
    if (reportPath_.empty())
    {
        return std::nullopt;
    }

    const std::optional<std::string> contents = filesystemProbe_.ContentsOf(reportPath_);
    if (!contents.has_value())
    {
        return std::nullopt;
    }

    LoadingReport report = LoadingReportFrom(*contents);
    if (!ItSaidSomething(report))
    {
        return std::nullopt;
    }

    return report;
}
