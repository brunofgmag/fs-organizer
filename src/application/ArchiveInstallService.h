#ifndef FS_ORGANIZER_APPLICATION_ARCHIVE_INSTALL_SERVICE_H
#define FS_ORGANIZER_APPLICATION_ARCHIVE_INSTALL_SERVICE_H

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

#include "application/ImportService.h"
#include "application/model/ArchiveInstall.h"
#include "domain/journal/OperationLog.h"
#include "domain/model/DestinationEntry.h"
#include "domain/model/OperationKind.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/ports/ArchiveReader.h"
#include "domain/ports/FileOperations.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/ManifestParser.h"

class ArchiveInstallService
{
public:
    ArchiveInstallService(const ArchiveReader& archives,
                          const ManifestParser& manifests,
                          const FilesystemProbe& filesystemProbe,
                          FileOperations& files,
                          const OperationLog& log,
                          const ImportService& importing);

    [[nodiscard]] bool IsAFolder(const std::filesystem::path& source) const;

    [[nodiscard]] WhatIsInTheArchive Inspect(const SimulatorProfile& profile,
                                             const std::filesystem::path& archive,
                                             const std::filesystem::path& category) const;

    [[nodiscard]] std::vector<std::vector<AdmissionCheck>>
    AdmissionIn(const SimulatorProfile& profile,
                const std::vector<PackageToInstall>& packages,
                const std::vector<std::filesystem::path>& categories) const;

    [[nodiscard]] WhatInstallingDid Install(const SimulatorProfile& profile,
                                            const std::vector<DestinationEntry>& entries,
                                            const ArchiveInstallRequest& request,
                                            const std::function<bool(const CopyProgress&)>& onProgress = {},
                                            const std::function<void(OperationKind)>& onStep = {},
                                            const std::function<bool(std::size_t package)>& goOn = {}) const;

private:
    struct Run;

    [[nodiscard]] bool TheReaderTakesItForAFolder(const std::filesystem::path& source) const;

    [[nodiscard]] std::string VersionIn(const ArchiveText& manifest) const;

    void CheckTheAdmissions(const SimulatorProfile& profile,
                            const std::filesystem::path& category,
                            WhatIsInTheArchive& found) const;

    [[nodiscard]] std::optional<WhatInstallingDid> ReadTheArchive(const ArchiveInstallRequest& request, Run& run) const;

    [[nodiscard]] bool ChooseThePackages(const ArchiveInstallRequest& request, Run& run) const;

    [[nodiscard]] FileResult
    AdmitTheChosen(const SimulatorProfile& profile, const ArchiveInstallRequest& request, Run& run) const;

    [[nodiscard]] FileResult
    PrepareTheStaging(const SimulatorProfile& profile, const std::filesystem::path& category, Run& run) const;

    [[nodiscard]] FileResult ExtractAndVerify(const SimulatorProfile& profile,
                                              const ArchiveInstallRequest& request,
                                              const std::function<bool(const CopyProgress&)>& onProgress,
                                              const std::function<void(OperationKind)>& onStep,
                                              Run& run) const;

    [[nodiscard]] bool TheStagingMatchesTheListing(const Run& run) const;

    [[nodiscard]] bool DiscardTheStaging(const SimulatorProfile& profile, const Run& run) const;

    [[nodiscard]] WhatInstallingDid ExtractPlaceAndTidyUp(const SimulatorProfile& profile,
                                                          const std::vector<DestinationEntry>& entries,
                                                          const ArchiveInstallRequest& request,
                                                          const std::function<bool(const CopyProgress&)>& onProgress,
                                                          const std::function<void(OperationKind)>& onStep,
                                                          const std::function<bool(std::size_t package)>& goOn,
                                                          Run& run) const;

    void PlaceTheSurvivors(const SimulatorProfile& profile,
                           const std::vector<DestinationEntry>& entries,
                           const ArchiveInstallRequest& request,
                           const std::function<bool(const CopyProgress&)>& onProgress,
                           const std::function<void(OperationKind)>& onStep,
                           const std::function<bool(std::size_t package)>& goOn,
                           Run& run) const;

    const ArchiveReader& archives_;
    const ManifestParser& manifests_;
    const FilesystemProbe& filesystemProbe_;
    FileOperations& files_;
    const OperationLog& log_;
    const ImportService& importing_;
};

#endif // FS_ORGANIZER_APPLICATION_ARCHIVE_INSTALL_SERVICE_H
