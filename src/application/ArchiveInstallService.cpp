#include "application/ArchiveInstallService.h"

#include <algorithm>
#include <string>
#include <utility>

#include "domain/importing/FingerprintMeasures.h"
#include "domain/importing/ImportPaths.h"
#include "domain/installing/PackagesInAnArchive.h"
#include "domain/model/FileFingerprint.h"
#include "domain/support/CaseFolding.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    constexpr std::size_t kMostOfAManifest = 1024 * 1024;

    std::filesystem::path WhereItWouldLand(const std::filesystem::path& category, const PackageInAnArchive& package)
    {
        return category / PathFromUtf8(FolderNameOf(package).Text());
    }

    FileResult ResultOf(const ArchiveFailure failure)
    {
        switch (failure)
        {
        case ArchiveFailure::None: return FileResult::Completed;
        case ArchiveFailure::Cancelled: return FileResult::Cancelled;
        case ArchiveFailure::NeedsAPassword: return FileResult::TheArchiveNeedsAPassword;
        case ArchiveFailure::TheToolIsMissing:
        case ArchiveFailure::CouldNotOpen:
        case ArchiveFailure::CouldNotExtract:
        case ArchiveFailure::TookTooLong: return FileResult::CouldNotExtract;
        }

        return FileResult::CouldNotExtract;
    }

    std::vector<FileFingerprint> FilesListedIn(const std::vector<ArchiveEntry>& entries)
    {
        std::vector<FileFingerprint> files;

        for (const ArchiveEntry& entry : entries)
        {
            if (entry.kind == ArchiveEntryKind::File)
            {
                files.push_back(FileFingerprint{.relativePath = PathFromUtf8(entry.path), .size = entry.bytes});
            }
        }

        return files;
    }

    std::vector<FileFingerprint> FoldedLikeTheDisk(std::vector<FileFingerprint> files)
    {
        for (FileFingerprint& file : files)
        {
            file.relativePath = PathFromUtf8(LoweredForComparison(AsUtf8(file.relativePath)));
        }

        return files;
    }

    bool TheOfferAgreesWithTheDisk(const AdmissionCheck& check, const ChosenPackage& chosen)
    {
        return ComparablePath(check.occupant) == ComparablePath(chosen.replaces);
    }

    bool TheSourceNestsWith(const std::filesystem::path& source, const std::filesystem::path& place)
    {
        return !place.empty() && (PathIsInside(source, place) || PathIsInside(place, source));
    }

    bool TheSourceWouldBeRemovedWithTheEarlierCopy(const AdmissionCheck& check,
                                                   const ChosenPackage& chosen,
                                                   const std::filesystem::path& source)
    {
        return chosen.discardTheEarlierQuarantinedCopy && TheSourceNestsWith(source, check.quarantinedCopy);
    }

    std::optional<FileResult> WhyThePackageTakesNoPart(const AdmissionCheck& check,
                                                       const ChosenPackage& chosen,
                                                       const std::filesystem::path& archive)
    {
        if (!Succeeded(check.result))
        {
            return check.result;
        }

        if (check.theQuarantineHoldsAnEarlierCopy && !chosen.discardTheEarlierQuarantinedCopy)
        {
            return FileResult::TheQuarantineIsOccupied;
        }

        if (!TheOfferAgreesWithTheDisk(check, chosen))
        {
            return FileResult::TheDiskDisagreesWithTheScan;
        }

        if (TheSourceNestsWith(archive, check.occupant)
            || TheSourceWouldBeRemovedWithTheEarlierCopy(check, chosen, archive))
        {
            return FileResult::TheSourceIsInsideWhatWouldBeRemoved;
        }

        return std::nullopt;
    }

    std::function<bool(std::size_t)> GoOnByChosenIndex(const std::function<bool(std::size_t)>& goOn,
                                                       const std::vector<std::size_t>& survivors)
    {
        if (!goOn)
        {
            return {};
        }

        return [&goOn, &survivors](const std::size_t placed)
        {
            return goOn(survivors[placed]);
        };
    }

    WhatInstallingDid Stopped(const FileResult reason)
    {
        return WhatInstallingDid{.stopped = reason};
    }

    AddonId TheStagingIsNoAddon(const SimulatorProfile& profile, const std::filesystem::path& staging)
    {
        AddonId identity = IdentityOf(profile, ImportedPathFor(staging));
        identity.folderName.clear();

        return identity;
    }
}

struct ArchiveInstallService::Run
{
    ArchiveListing listing{};
    WhatAnArchiveHolds holds{};
    std::vector<const PackageInAnArchive*> chosen{};
    std::vector<PackageInstalled> installed{};
    std::vector<std::size_t> survivors{};
    std::filesystem::path staging{};
    ArchiveFailure extractionFailure = ArchiveFailure::None;
};

ArchiveInstallService::ArchiveInstallService(const ArchiveReader& archives,
                                             const ManifestParser& manifests,
                                             const FilesystemProbe& filesystemProbe,
                                             FileOperations& files,
                                             const OperationLog& log,
                                             const ImportService& importing)
    : archives_(archives),
      manifests_(manifests),
      filesystemProbe_(filesystemProbe),
      files_(files),
      log_(log),
      importing_(importing)
{
}

bool ArchiveInstallService::IsAFolder(const std::filesystem::path& source) const
{
    return filesystemProbe_.PhysicalDirectoryExists(source);
}

bool ArchiveInstallService::TheReaderTakesItForAFolder(const std::filesystem::path& source) const
{
    return IsAFolder(source)
        || (filesystemProbe_.IsReparsePoint(source) && filesystemProbe_.TargetDirectoryExists(source));
}

WhatIsInTheArchive ArchiveInstallService::Inspect(const SimulatorProfile& profile,
                                                  const std::filesystem::path& archive,
                                                  const std::filesystem::path& category) const
{
    const bool aFolder = TheReaderTakesItForAFolder(archive);
    const ArchiveListing listing = archives_.EntriesOf(archive);

    if (listing.failure != ArchiveFailure::None)
    {
        return WhatIsInTheArchive{.failure = listing.failure, .theSourceIsAFolder = aFolder};
    }

    const WhatAnArchiveHolds holds = PackagesIn(listing.entries);

    if (holds.refusal != NothingToInstall::None)
    {
        return WhatIsInTheArchive{.refusal = holds.refusal, .theSourceIsAFolder = aFolder};
    }

    WhatIsInTheArchive found{.bytes = holds.bytes, .theSourceIsAFolder = aFolder};

    for (const PackageInAnArchive& package : holds.packages)
    {
        const ArchiveText manifest = archives_.ContentsOf(archive, package.manifest, kMostOfAManifest);

        if (manifest.failure == ArchiveFailure::NeedsAPassword)
        {
            return WhatIsInTheArchive{.failure = ArchiveFailure::NeedsAPassword, .theSourceIsAFolder = aFolder};
        }

        found.packages.push_back(PackageToInstall{.package = package, .versionInTheArchive = VersionIn(manifest)});
    }

    if (!category.empty())
    {
        CheckTheAdmissions(profile, category, found);
        found.room = RoomFor(filesystemProbe_, category, holds.bytes, holds.files);
    }

    return found;
}

std::string ArchiveInstallService::VersionIn(const ArchiveText& manifest) const
{
    if (manifest.failure != ArchiveFailure::None)
    {
        return {};
    }

    const std::optional<Manifest> parsed = manifests_.Parse(manifest.text);

    return parsed.has_value() ? parsed->packageVersion : std::string{};
}

std::vector<std::vector<AdmissionCheck>>
ArchiveInstallService::AdmissionIn(const SimulatorProfile& profile,
                                   const std::vector<PackageToInstall>& packages,
                                   const std::vector<std::filesystem::path>& categories) const
{
    std::vector<std::vector<AdmissionCheck>> inEachCategory(categories.size());

    if (packages.empty() || categories.empty())
    {
        return inEachCategory;
    }

    std::vector<std::filesystem::path> wanted;
    wanted.reserve(categories.size() * packages.size());

    for (const std::filesystem::path& category : categories)
    {
        for (const PackageToInstall& candidate : packages)
        {
            wanted.push_back(WhereItWouldLand(category, candidate.package));
        }
    }

    const std::vector<AdmissionCheck> checks = importing_.CheckAdmission(profile, wanted);
    const auto perCategory = static_cast<std::ptrdiff_t>(packages.size());

    for (std::size_t category = 0; category < categories.size(); ++category)
    {
        const auto from = checks.begin() + static_cast<std::ptrdiff_t>(category) * perCategory;

        inEachCategory[category].assign(from, from + perCategory);
    }

    return inEachCategory;
}

void ArchiveInstallService::CheckTheAdmissions(const SimulatorProfile& profile,
                                               const std::filesystem::path& category,
                                               WhatIsInTheArchive& found) const
{
    const std::vector<AdmissionCheck> checks = AdmissionIn(profile, found.packages, {category}).front();

    for (std::size_t index = 0; index < found.packages.size(); ++index)
    {
        found.packages[index].admission = checks[index];
    }
}

WhatInstallingDid ArchiveInstallService::Install(const SimulatorProfile& profile,
                                                 const std::vector<DestinationEntry>& entries,
                                                 const ArchiveInstallRequest& request,
                                                 const std::function<bool(const CopyProgress&)>& onProgress,
                                                 const std::function<void(OperationKind)>& onStep,
                                                 const std::function<bool(std::size_t package)>& goOn) const
{
    Run run;

    if (const std::optional<WhatInstallingDid> ended = ReadTheArchive(request, run))
    {
        return *ended;
    }

    if (!ChooseThePackages(request, run))
    {
        return Stopped(FileResult::TheDiskDisagreesWithTheScan);
    }

    if (run.chosen.empty())
    {
        return WhatInstallingDid{};
    }

    if (TheSourceNestsWith(request.archive, InstallStagingIn(request.category)))
    {
        return Stopped(FileResult::TheSourceIsInsideWhatWouldBeRemoved);
    }

    if (const FileResult admitted = AdmitTheChosen(profile, request, run); !Succeeded(admitted))
    {
        return Stopped(admitted);
    }

    if (run.survivors.empty())
    {
        return WhatInstallingDid{.packages = std::move(run.installed)};
    }

    if (const FileResult room = RoomFor(filesystemProbe_, request.category, run.holds.bytes, run.holds.files);
        !Succeeded(room))
    {
        return Stopped(room);
    }

    if (const FileResult prepared = PrepareTheStaging(profile, request.category, run); !Succeeded(prepared))
    {
        return Stopped(prepared);
    }

    return ExtractPlaceAndTidyUp(profile, entries, request, onProgress, onStep, goOn, run);
}

WhatInstallingDid
ArchiveInstallService::ExtractPlaceAndTidyUp(const SimulatorProfile& profile,
                                             const std::vector<DestinationEntry>& entries,
                                             const ArchiveInstallRequest& request,
                                             const std::function<bool(const CopyProgress&)>& onProgress,
                                             const std::function<void(OperationKind)>& onStep,
                                             const std::function<bool(std::size_t package)>& goOn,
                                             Run& run) const
{
    const FileResult unfit = ExtractAndVerify(profile, request, onProgress, onStep, run);

    if (Succeeded(unfit))
    {
        PlaceTheSurvivors(profile, entries, request, onProgress, onStep, goOn, run);
    }

    const bool theStagingStayed = !DiscardTheStaging(profile, run);

    if (run.extractionFailure == ArchiveFailure::TookTooLong)
    {
        return WhatInstallingDid{.failure = ArchiveFailure::TookTooLong, .theStagingStayed = theStagingStayed};
    }

    if (!Succeeded(unfit))
    {
        return WhatInstallingDid{.stopped = unfit, .theStagingStayed = theStagingStayed};
    }

    return WhatInstallingDid{.packages = std::move(run.installed), .theStagingStayed = theStagingStayed};
}

std::optional<WhatInstallingDid> ArchiveInstallService::ReadTheArchive(const ArchiveInstallRequest& request,
                                                                       Run& run) const
{
    run.listing = archives_.EntriesOf(request.archive);

    if (run.listing.failure != ArchiveFailure::None)
    {
        return WhatInstallingDid{.failure = run.listing.failure};
    }

    run.holds = PackagesIn(run.listing.entries);

    if (run.holds.refusal != NothingToInstall::None)
    {
        return WhatInstallingDid{.refusal = run.holds.refusal};
    }

    return std::nullopt;
}

bool ArchiveInstallService::ChooseThePackages(const ArchiveInstallRequest& request, Run& run) const
{
    for (const ChosenPackage& wanted : request.packages)
    {
        const auto found = std::ranges::find_if(run.holds.packages,
                                                [&wanted](const PackageInAnArchive& package)
                                                {
                                                    return EntryPathOf(package) == wanted.root;
                                                });

        if (found == run.holds.packages.end())
        {
            return false;
        }

        run.chosen.push_back(&*found);
    }

    return true;
}

FileResult ArchiveInstallService::AdmitTheChosen(const SimulatorProfile& profile,
                                                 const ArchiveInstallRequest& request,
                                                 Run& run) const
{
    std::vector<std::filesystem::path> wanted;
    wanted.reserve(run.chosen.size());

    for (const PackageInAnArchive* package : run.chosen)
    {
        wanted.push_back(WhereItWouldLand(request.category, *package));
    }

    const std::vector<AdmissionCheck> checks = importing_.CheckAdmission(profile, wanted);

    if (std::ranges::any_of(checks,
                            [](const AdmissionCheck& check)
                            {
                                return check.result == FileResult::TheSimulatorIsRunning;
                            }))
    {
        return FileResult::TheSimulatorIsRunning;
    }

    for (std::size_t index = 0; index < run.chosen.size(); ++index)
    {
        PackageInstalled installed{.root = request.packages[index].root};
        const std::optional<FileResult> refused =
            WhyThePackageTakesNoPart(checks[index], request.packages[index], request.archive);

        if (refused.has_value())
        {
            installed.result = *refused;
        }
        else
        {
            run.survivors.push_back(index);
        }

        run.installed.push_back(std::move(installed));
    }

    return FileResult::Completed;
}

FileResult ArchiveInstallService::PrepareTheStaging(const SimulatorProfile& profile,
                                                    const std::filesystem::path& category,
                                                    Run& run) const
{
    run.staging = InstallStagingIn(category);

    if (filesystemProbe_.EntryExistsWithoutFollowingLinks(run.staging))
    {
        const FileResult discarded =
            files_.RemoveTree(run.staging) ? FileResult::Completed : FileResult::CouldNotDiscard;

        log_.RecordImport(OperationKind::DiscardStaging, TheStagingIsNoAddon(profile, run.staging), run.staging, {},
                          discarded);

        if (!Succeeded(discarded))
        {
            return discarded;
        }
    }

    return files_.CreateFolder(run.staging) ? FileResult::Completed : FileResult::CouldNotExtract;
}

FileResult ArchiveInstallService::ExtractAndVerify(const SimulatorProfile& profile,
                                                   const ArchiveInstallRequest& request,
                                                   const std::function<bool(const CopyProgress&)>& onProgress,
                                                   const std::function<void(OperationKind)>& onStep,
                                                   Run& run) const
{
    if (onStep)
    {
        onStep(OperationKind::InstallExtractToStaging);
    }

    run.extractionFailure = archives_.ExtractAll(request.archive, run.listing.entries, run.staging, onProgress);

    FileResult settled = ResultOf(run.extractionFailure);

    if (Succeeded(settled))
    {
        if (onStep)
        {
            onStep(OperationKind::ImportVerifyStaging);
        }

        settled = TheStagingMatchesTheListing(run) ? FileResult::Completed : FileResult::VerificationFailed;
    }

    log_.RecordImport(OperationKind::InstallExtractToStaging, TheStagingIsNoAddon(profile, run.staging),
                      request.archive, run.staging, settled, OriginSource::Unknown, AsUtf8(request.archive.filename()));

    return settled;
}

bool ArchiveInstallService::TheStagingMatchesTheListing(const Run& run) const
{
    const std::optional<TreeFingerprint> walked = filesystemProbe_.FingerprintTree(run.staging);

    return walked.has_value()
        && FingerprintsMatch(FoldedLikeTheDisk(walked->files), FoldedLikeTheDisk(FilesListedIn(run.listing.entries)));
}

bool ArchiveInstallService::DiscardTheStaging(const SimulatorProfile& profile, const Run& run) const
{
    if (files_.RemoveTree(run.staging))
    {
        return true;
    }

    log_.RecordImport(OperationKind::DiscardStaging, TheStagingIsNoAddon(profile, run.staging), run.staging, {},
                      FileResult::CouldNotDiscard);

    return false;
}

void ArchiveInstallService::PlaceTheSurvivors(const SimulatorProfile& profile,
                                              const std::vector<DestinationEntry>& entries,
                                              const ArchiveInstallRequest& request,
                                              const std::function<bool(const CopyProgress&)>& onProgress,
                                              const std::function<void(OperationKind)>& onStep,
                                              const std::function<bool(std::size_t package)>& goOn,
                                              Run& run) const
{
    std::vector<AdmitRequest> admissions;

    for (const std::size_t index : run.survivors)
    {
        const ChosenPackage& chosen = request.packages[index];

        admissions.push_back(AdmitRequest{.staged = PathUnder(run.staging, PathFromUtf8(chosen.root)),
                                          .category = request.category,
                                          .replaces = chosen.replaces,
                                          .discardTheEarlierQuarantinedCopy = chosen.discardTheEarlierQuarantinedCopy});
    }

    if (onStep)
    {
        onStep(OperationKind::InstallMoveIntoPlace);
    }

    const std::vector<AdmitResult> admitted =
        importing_.Admit(profile, entries, admissions, onProgress, onStep, GoOnByChosenIndex(goOn, run.survivors));

    for (std::size_t placed = 0; placed < admitted.size(); ++placed)
    {
        PackageInstalled& installed = run.installed[run.survivors[placed]];

        installed.result = admitted[placed].result;
        installed.landed = admitted[placed].landed;
        installed.replaced = admitted[placed].replaced;
    }
}
