#ifndef FS_ORGANIZER_APPLICATION_MODEL_ARCHIVE_INSTALL_H
#define FS_ORGANIZER_APPLICATION_MODEL_ARCHIVE_INSTALL_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "application/model/Admission.h"
#include "domain/installing/PackagesInAnArchive.h"
#include "domain/model/ArchiveEntry.h"
#include "domain/model/FileResult.h"

struct PackageToInstall
{
    PackageInAnArchive package{};
    std::string versionInTheArchive{};
    AdmissionCheck admission{};
};

struct WhatIsInTheArchive
{
    ArchiveFailure failure = ArchiveFailure::None;
    NothingToInstall refusal = NothingToInstall::None;
    FileResult room = FileResult::Completed;
    std::vector<PackageToInstall> packages{};
    std::uintmax_t bytes = 0;
    bool theSourceIsAFolder = false;
};

struct ChosenPackage
{
    std::string root{};
    bool discardTheEarlierQuarantinedCopy = false;
    std::filesystem::path replaces{};
};

struct ArchiveInstallRequest
{
    std::filesystem::path archive{};
    std::filesystem::path category{};
    std::vector<ChosenPackage> packages{};
};

struct PackageInstalled
{
    std::string root{};
    FileResult result = FileResult::TheOutcomeIsUnknown;
    std::filesystem::path landed{};
    bool replaced = false;
};

struct WhatInstallingDid
{
    ArchiveFailure failure = ArchiveFailure::None;
    NothingToInstall refusal = NothingToInstall::None;
    FileResult stopped = FileResult::Completed;
    std::vector<PackageInstalled> packages{};
    bool theStagingStayed = false;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_ARCHIVE_INSTALL_H
