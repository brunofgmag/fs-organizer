#ifndef FS_ORGANIZER_DOMAIN_INSTALLING_PACKAGES_IN_AN_ARCHIVE_H
#define FS_ORGANIZER_DOMAIN_INSTALLING_PACKAGES_IN_AN_ARCHIVE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "domain/model/ArchiveEntry.h"
#include "domain/support/PathSegment.h"

enum class NothingToInstall : int
{
    None = 0,
    NoPackage = 1,
    OnlyAnInstaller = 2,
    AnEntryIsNotSafe = 3,
    ThePackageHasNoFolder = 4,
    TwoPackagesShareAName = 5,
    ThePackageNameIsReserved = 6,
};

struct PackageInAnArchive
{
    std::vector<PathSegment> root{};
    std::string manifest{};
    std::uintmax_t bytes = 0;
    std::size_t files = 0;
};

struct WhatAnArchiveHolds
{
    NothingToInstall refusal = NothingToInstall::None;
    std::vector<PackageInAnArchive> packages{};
    std::uintmax_t bytes = 0;
    std::size_t files = 0;
};

[[nodiscard]] WhatAnArchiveHolds PackagesIn(const std::vector<ArchiveEntry>& entries);

[[nodiscard]] const PathSegment& FolderNameOf(const PackageInAnArchive& package);

[[nodiscard]] std::string EntryPathOf(const PackageInAnArchive& package);

#endif // FS_ORGANIZER_DOMAIN_INSTALLING_PACKAGES_IN_AN_ARCHIVE_H
