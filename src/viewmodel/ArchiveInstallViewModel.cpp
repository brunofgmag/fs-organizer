#include "viewmodel/ArchiveInstallViewModel.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "domain/installing/PackagesInAnArchive.h"
#include "domain/tree/AddonTree.h"
#include "support/PathText.h"
#include "viewmodel/ArchiveRefusalText.h"

namespace
{
    struct ArchiveToInstall
    {
        std::filesystem::path archive{};
        QString fileName{};
        SourceKind source = SourceKind::Archive;
        std::vector<ChosenAddon> chosen{};
    };

    SourceKind SourceKindOf(const bool aFolder)
    {
        return aFolder ? SourceKind::Folder : SourceKind::Archive;
    }

    QString FileNameOf(const std::filesystem::path& archive)
    {
        return AsText(archive.filename());
    }

    QString LibraryShown(const Library& library)
    {
        if (library.label.empty())
        {
            return AsText(library.path);
        }

        return QStringLiteral("%1 · %2").arg(QString::fromStdString(library.label), AsText(library.path));
    }

    QString CategoryShown(const std::filesystem::path& library, const std::filesystem::path& folder)
    {
        const std::filesystem::path relative = folder.lexically_relative(library);

        if (relative.empty())
        {
            return AsText(folder);
        }

        return relative == "." ? QObject::tr("(library root)") : AsText(relative);
    }

    InstallStandingInALibrary StandingFrom(const AdmissionCheck& check, const std::filesystem::path& library)
    {
        if (check.result == FileResult::TheOriginIsOccupied)
        {
            return InstallStandingInALibrary{.standing = InstallStanding::AFolderIsInTheWay};
        }

        if (!Succeeded(check.result) || check.occupant.empty())
        {
            return {};
        }

        InstallStandingInALibrary standing{.standing = InstallStanding::Replaces,
                                           .installedVersion = QString::fromStdString(check.occupantVersion),
                                           .installedIn = CategoryShown(library, check.occupant.parent_path()),
                                           .occupant = AsText(check.occupant)};

        if (check.theQuarantineHoldsAnEarlierCopy)
        {
            standing.standing = InstallStanding::ReplacesAndTheQuarantineHoldsOne;
            standing.quarantinedVersion = QString::fromStdString(check.quarantinedVersion);
        }

        return standing;
    }

    std::vector<std::vector<InstallStandingInALibrary>>
    StandingsAmong(const std::vector<InstallLibraryChoice>& libraries,
                   const std::vector<std::vector<std::vector<AdmissionCheck>>>& checks,
                   const std::size_t package)
    {
        std::vector<std::vector<InstallStandingInALibrary>> standings;

        for (std::size_t library = 0; library < libraries.size(); ++library)
        {
            std::vector<InstallStandingInALibrary> inEachCategory;

            for (const std::vector<AdmissionCheck>& category : checks[library])
            {
                inEachCategory.push_back(StandingFrom(category[package], AsPath(libraries[library].path)));
            }

            standings.push_back(std::move(inEachCategory));
        }

        return standings;
    }

    InstallStandingInALibrary StandingInEveryCategory(const std::vector<InstallStandingInALibrary>& inEachCategory)
    {
        if (inEachCategory.empty())
        {
            return {};
        }

        const bool alike = std::ranges::all_of(inEachCategory,
                                               [&inEachCategory](const InstallStandingInALibrary& standing)
                                               {
                                                   return standing.standing == inEachCategory.front().standing;
                                               });

        return alike ? inEachCategory.front() : InstallStandingInALibrary{};
    }

    std::vector<std::filesystem::path> CategoriesOf(const std::vector<InstallLibraryChoice>& libraries)
    {
        std::vector<std::filesystem::path> categories;

        for (const InstallLibraryChoice& library : libraries)
        {
            for (const InstallCategoryChoice& category : library.categories)
            {
                categories.push_back(AsPath(category.path));
            }
        }

        return categories;
    }

    QString WhyNothingIsOfferedFrom(const WhatIsInTheArchive& found, const QString& fileName)
    {
        const SourceKind source = SourceKindOf(found.theSourceIsAFolder);

        if (found.failure != ArchiveFailure::None)
        {
            return WhyTheArchiveIsRefused(found.failure, fileName, source);
        }

        return WhyTheArchiveIsRefused(found.refusal, source);
    }

    std::vector<InstallLibraryChoice> LibrariesOffered(const SimulatorProfile& profile,
                                                       const std::vector<TreeNode>& trees)
    {
        std::vector<InstallLibraryChoice> offered;

        for (const Library& library : profile.libraries)
        {
            InstallLibraryChoice choice{.shown = LibraryShown(library), .path = AsText(library.path)};

            if (const TreeNode* tree = LibraryTreeAt(trees, library.path))
            {
                for (const TreeNode* folder : CategoriesOfferedIn(*tree, true))
                {
                    choice.categories.push_back(InstallCategoryChoice{
                        .shown = CategoryShown(library.path, folder->path), .path = AsText(folder->path)});
                }
            }

            offered.push_back(std::move(choice));
        }

        return offered;
    }

    std::vector<ArchiveToInstall> ArchivesToInstall(const std::vector<ChosenAddon>& chosen)
    {
        std::vector<ChosenAddon> byArchive = chosen;
        std::ranges::stable_sort(byArchive,
                                 [](const ChosenAddon& one, const ChosenAddon& other)
                                 {
                                     return one.archive < other.archive;
                                 });

        std::vector<ArchiveToInstall> archives;
        int current = -1;

        for (ChosenAddon& addon : byArchive)
        {
            if (archives.empty() || addon.archive != current)
            {
                current = addon.archive;
                archives.push_back(ArchiveToInstall{.archive = AsPath(addon.archivePath),
                                                    .fileName = FileNameOf(AsPath(addon.archivePath)),
                                                    .source = addon.source});
            }

            archives.back().chosen.push_back(std::move(addon));
        }

        return archives;
    }

    ArchiveInstallRequest RequestFor(const ArchiveToInstall& archive, const std::filesystem::path& category)
    {
        ArchiveInstallRequest request{.archive = archive.archive, .category = category};

        for (const ChosenAddon& addon : archive.chosen)
        {
            request.packages.push_back(
                ChosenPackage{.root = addon.root.toStdString(),
                              .discardTheEarlierQuarantinedCopy = addon.discardsTheQuarantinedCopy,
                              .replaces = AsPath(addon.replaces)});
        }

        return request;
    }

    InstallStep StepThatBringsTheFilesIn(const SourceKind source)
    {
        return source == SourceKind::Folder ? InstallStep::Copying : InstallStep::Extracting;
    }

    std::optional<InstallStep> StepOf(const OperationKind kind, const SourceKind source)
    {
        switch (kind)
        {
        case OperationKind::InstallExtractToStaging: return StepThatBringsTheFilesIn(source);
        case OperationKind::ImportVerifyStaging: return InstallStep::Checking;
        case OperationKind::InstallMoveIntoPlace: return InstallStep::PuttingIntoPlace;
        default: return std::nullopt;
        }
    }

    std::optional<QString> WhyTheWholeArchiveStopped(const WhatInstallingDid& did, const ArchiveToInstall& archive)
    {
        if (did.failure != ArchiveFailure::None)
        {
            return WhyTheArchiveIsRefused(did.failure, archive.fileName, archive.source);
        }

        if (did.refusal != NothingToInstall::None)
        {
            return WhyTheArchiveIsRefused(did.refusal, archive.source);
        }

        if (!Succeeded(did.stopped))
        {
            return WhyItWasNotInstalled(did.stopped, archive.source);
        }

        return std::nullopt;
    }

    void NoteTheAddons(const ArchiveToInstall& archive,
                       const WhatInstallingDid& did,
                       InstallReportArchive& lines,
                       InstallOutcome& outcome)
    {
        for (std::size_t index = 0; index < archive.chosen.size(); ++index)
        {
            const PackageInstalled placed = index < did.packages.size() ? did.packages[index] : PackageInstalled{};

            if (TheFolderLanded(placed.result))
            {
                ++outcome.installed;
                outcome.landed.push_back(placed.landed);
            }
            else
            {
                ++outcome.report->notInstalled;
            }

            if (!Succeeded(placed.result))
            {
                lines.lines.push_back(InstallReportLine{.addon = archive.chosen[index].folderName,
                                                        .reason = WhyItWasNotInstalled(placed.result, archive.source)});
            }
        }
    }

    void NoteTheArchive(const ArchiveToInstall& archive,
                        const std::optional<WhatInstallingDid>& result,
                        InstallOutcome& outcome)
    {
        InstallReportArchive lines{.fileName = archive.fileName};
        const WhatInstallingDid did = result.value_or(WhatInstallingDid{.stopped = FileResult::Cancelled});

        if (const std::optional<QString> why = WhyTheWholeArchiveStopped(did, archive))
        {
            outcome.report->notInstalled += static_cast<int>(archive.chosen.size());
            lines.lines.push_back(InstallReportLine{.reason = *why});
        }
        else
        {
            NoteTheAddons(archive, did, lines, outcome);
        }

        if (did.theStagingStayed)
        {
            lines.leftBehind = TheExtractionFolderStayed(archive.source);
        }

        if (!lines.lines.empty() || !lines.leftBehind.isEmpty())
        {
            outcome.report->archives.push_back(std::move(lines));
        }
    }

    bool SomeArchiveKeptALine(const InstallReport& report)
    {
        return std::ranges::any_of(report.archives,
                                   [](const InstallReportArchive& archive)
                                   {
                                       return !archive.lines.empty();
                                   });
    }

    InstallReportKind KindOfTheReport(const InstallReport& report)
    {
        if (report.notInstalled > 0)
        {
            return report.installed == 0 ? InstallReportKind::NothingWasInstalled
                                         : InstallReportKind::SomeWereNotInstalled;
        }

        return SomeArchiveKeptALine(report) ? InstallReportKind::InstalledButNotLinked
                                            : InstallReportKind::InstalledButAFolderStayed;
    }

    InstallOutcome OutcomeOf(const std::vector<ArchiveToInstall>& archives,
                             const std::vector<std::optional<WhatInstallingDid>>& results,
                             const bool enableAfter)
    {
        InstallOutcome outcome{.report = InstallReport{}, .enableAfter = enableAfter};

        for (std::size_t index = 0; index < archives.size(); ++index)
        {
            NoteTheArchive(archives[index], results[index], outcome);
        }

        InstallReport& report = *outcome.report;
        report.installed = outcome.installed;
        report.kind = KindOfTheReport(report);

        if (report.notInstalled == 0 && report.archives.empty())
        {
            outcome.report.reset();
        }

        return outcome;
    }
}

ArchiveInstallViewModel::ArchiveInstallViewModel(const ArchiveInstallService& service,
                                                 ProfileService& profileService,
                                                 const ProcessProbe& probe,
                                                 Session& session,
                                                 const SessionNotifier& notifier,
                                                 BackgroundRunner& runner,
                                                 QObject* parent)
    : QObject(parent),
      service_(service),
      profileService_(profileService),
      probe_(probe),
      session_(session),
      notifier_(notifier),
      working_(runner)
{
}

bool ArchiveInstallViewModel::Busy() const
{
    return working_.Busy();
}

bool ArchiveInstallViewModel::IsAFolder(const std::filesystem::path& source) const
{
    return service_.IsAFolder(source);
}

void ArchiveInstallViewModel::Cancel()
{
    if (!working_.Busy())
    {
        return;
    }

    cancelled_ = true;

    if (installing_)
    {
        emit Cancelling();
    }
}

void ArchiveInstallViewModel::Read(const std::vector<std::filesystem::path>& archives,
                                   const std::filesystem::path& droppedOnCategory)
{
    if (archives.empty())
    {
        return;
    }

    const SimulatorProfile profile = session_.Profile();
    const std::vector<InstallLibraryChoice> libraries = LibrariesOffered(profile, session_.Snapshot().libraries);

    struct Reading
    {
        std::vector<ArchiveRead> reads{};
        Standings standings{};
    };

    const auto reading = std::make_shared<Reading>();

    working_.Run(
        [this]
        {
            cancelled_ = false;
            installing_ = false;
        },
        [this, profile, libraries, archives, reading]
        {
            for (std::size_t index = 0; index < archives.size() && !cancelled_; ++index)
            {
                emit ReadingArchive(static_cast<int>(index) + 1, static_cast<int>(archives.size()),
                                    FileNameOf(archives[index]));

                reading->reads.push_back(
                    ArchiveRead{.archive = archives[index], .found = service_.Inspect(profile, archives[index], {})});
                reading->standings.ofEachArchive.push_back(StandingOf(profile, libraries, reading->reads.back()));
            }

            reading->standings.runningSimulator = probe_.RunningSimulator().value_or(std::string{});
        },
        [this, libraries, droppedOnCategory, reading]
        {
            if (cancelled_)
            {
                emit ReadingStopped();
                return;
            }

            read_ = std::move(reading->reads);
            droppedOn_ = droppedOnCategory;

            emit OfferReady(OfferOf(libraries, read_, reading->standings));
        });
}

void ArchiveInstallViewModel::CheckAgain()
{
    if (read_.empty())
    {
        return;
    }

    const SimulatorProfile profile = session_.Profile();
    const std::vector<InstallLibraryChoice> libraries = LibrariesOffered(profile, session_.Snapshot().libraries);
    const auto standings = std::make_shared<Standings>();

    working_.Run(
        [this]
        {
            cancelled_ = false;
            installing_ = false;
        },
        [this, profile, libraries, reads = read_, standings]
        {
            *standings = StandingsOfAll(profile, libraries, reads);
        },
        [this, libraries, standings]
        {
            emit OfferReady(OfferOf(libraries, read_, *standings));
        });
}

std::vector<std::vector<std::vector<AdmissionCheck>>>
ArchiveInstallViewModel::StandingOf(const SimulatorProfile& profile,
                                    const std::vector<InstallLibraryChoice>& libraries,
                                    const ArchiveRead& read) const
{
    std::vector<std::vector<AdmissionCheck>> inEachCategory =
        service_.AdmissionIn(profile, read.found.packages, CategoriesOf(libraries));
    std::vector<std::vector<std::vector<AdmissionCheck>>> inEachLibrary;
    auto next = inEachCategory.begin();

    for (const InstallLibraryChoice& library : libraries)
    {
        const auto end = next + static_cast<std::ptrdiff_t>(library.categories.size());

        inEachLibrary.emplace_back(std::make_move_iterator(next), std::make_move_iterator(end));
        next = end;
    }

    return inEachLibrary;
}

ArchiveInstallViewModel::Standings
ArchiveInstallViewModel::StandingsOfAll(const SimulatorProfile& profile,
                                        const std::vector<InstallLibraryChoice>& libraries,
                                        const std::vector<ArchiveRead>& reads) const
{
    Standings standings;

    for (const ArchiveRead& read : reads)
    {
        standings.ofEachArchive.push_back(StandingOf(profile, libraries, read));
    }

    standings.runningSimulator = probe_.RunningSimulator().value_or(std::string{});

    return standings;
}

InstallOffer ArchiveInstallViewModel::OfferOf(const std::vector<InstallLibraryChoice>& libraries,
                                              const std::vector<ArchiveRead>& reads,
                                              const Standings& standings) const
{
    InstallOffer offer{.libraries = libraries,
                       .droppedOn = AsText(droppedOn_),
                       .runningSimulator = QString::fromStdString(standings.runningSimulator)};

    for (std::size_t index = 0; index < reads.size(); ++index)
    {
        const ArchiveRead& read = reads[index];
        InstallArchiveLine line{.fileName = FileNameOf(read.archive),
                                .path = AsText(read.archive),
                                .source = SourceKindOf(read.found.theSourceIsAFolder)};

        line.refusal = WhyNothingIsOfferedFrom(read.found, line.fileName);

        for (std::size_t package = 0; package < read.found.packages.size(); ++package)
        {
            const PackageToInstall& candidate = read.found.packages[package];
            InstallAddonLine addon{.folderName = QString::fromStdString(FolderNameOf(candidate.package).Text()),
                                   .version = QString::fromStdString(candidate.versionInTheArchive),
                                   .bytes = candidate.package.bytes,
                                   .root = QString::fromStdString(EntryPathOf(candidate.package))};

            addon.inEachCategory = StandingsAmong(libraries, standings.ofEachArchive[index], package);

            for (const std::vector<InstallStandingInALibrary>& inEachCategory : addon.inEachCategory)
            {
                addon.inEachLibrary.push_back(StandingInEveryCategory(inEachCategory));
            }

            line.addons.push_back(std::move(addon));
        }

        offer.archives.push_back(std::move(line));
    }

    return offer;
}

InstallReport ArchiveInstallViewModel::ReportOfWhatCannotBeInstalled(const InstallOffer& offer)
{
    InstallReport report{.kind = InstallReportKind::NothingToInstall};

    for (const InstallArchiveLine& archive : offer.archives)
    {
        if (!archive.refusal.isEmpty())
        {
            report.archives.push_back(InstallReportArchive{.fileName = archive.fileName,
                                                           .lines = {InstallReportLine{.reason = archive.refusal}}});
        }
    }

    return report;
}

std::function<void(OperationKind)> ArchiveInstallViewModel::OnStep(const SourceKind source)
{
    return [this, source](const OperationKind kind)
    {
        if (const std::optional<InstallStep> step = StepOf(kind, source))
        {
            emit StepChanged(*step);
        }
    };
}

std::function<bool(const CopyProgress&)> ArchiveInstallViewModel::OnProgress()
{
    return [this](const CopyProgress& progress)
    {
        emit Progressed(progress.copiedBytes, progress.totalBytes);

        return !cancelled_;
    };
}

void ArchiveInstallViewModel::Install(const std::vector<ChosenAddon>& chosen,
                                      const QString& categoryPath,
                                      const bool enableAfter)
{
    if (chosen.empty())
    {
        return;
    }

    const SimulatorProfile profile = session_.Profile();
    const std::vector<DestinationEntry> entries = session_.Snapshot().entries;
    const std::vector<ArchiveToInstall> archives = ArchivesToInstall(chosen);
    const std::filesystem::path category = AsPath(categoryPath);
    const auto results = std::make_shared<std::vector<std::optional<WhatInstallingDid>>>(archives.size());

    working_.Run(
        [this]
        {
            cancelled_ = false;
            installing_ = true;
        },
        [this, profile, entries, archives, category, results]
        {
            for (std::size_t index = 0; index < archives.size() && !cancelled_; ++index)
            {
                const ArchiveToInstall& archive = archives[index];

                emit InstallingArchive(static_cast<int>(index) + 1, static_cast<int>(archives.size()),
                                       archive.fileName);

                (*results)[index] = service_.Install(
                    profile, entries, RequestFor(archive, category), OnProgress(), OnStep(archive.source),
                    [this, &archive](const std::size_t addon)
                    {
                        if (cancelled_)
                        {
                            return false;
                        }

                        emit InstallingAddon(static_cast<int>(addon) + 1, static_cast<int>(archive.chosen.size()),
                                             archive.chosen[addon].folderName);

                        return true;
                    });
            }
        },
        [this, archives, results, enableAfter]
        {
            installing_ = false;

            AdoptTheEnd(OutcomeOf(archives, *results, enableAfter));
        });
}

void ArchiveInstallViewModel::AdoptTheEnd(const InstallOutcome& outcome)
{
    if (!outcome.landed.empty())
    {
        profileService_.ForgetUndo();
    }

    emit Finished(outcome);

    ReadTheLibraryAgain(outcome);
}

void ArchiveInstallViewModel::ReadTheLibraryAgain(const InstallOutcome& outcome)
{
    if (outcome.enableAfter && !outcome.landed.empty())
    {
        connect(
            &notifier_, &SessionNotifier::ScanFinished, this,
            [this, folders = outcome.landed]
            {
                emit LandedAndReadable(folders);
            },
            Qt::SingleShotConnection);
    }

    session_.ShowActiveProfile();
}
