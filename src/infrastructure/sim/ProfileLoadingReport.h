#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_PROFILE_LOADING_REPORT_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_PROFILE_LOADING_REPORT_H

#include <filesystem>
#include <optional>

#include "application/ports/LoadingReportSource.h"
#include "domain/ports/FilesystemProbe.h"

class ProfileLoadingReport final : public LoadingReportSource
{
public:
    ProfileLoadingReport(const FilesystemProbe& filesystemProbe, std::filesystem::path reportPath);

    void Use(std::filesystem::path reportPath);

    [[nodiscard]] std::optional<LoadingReport> LastReport() const override;

private:
    const FilesystemProbe& filesystemProbe_;
    std::filesystem::path reportPath_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_PROFILE_LOADING_REPORT_H
