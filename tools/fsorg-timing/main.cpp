#include <QtCore/QThread>
#include <QtWidgets/QApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTextStream>

#include <algorithm>
#include <optional>
#include <vector>

#include "application/ImportService.h"
#include "application/LibraryOrganizer.h"
#include "application/ProfileService.h"
#include "application/DeletionService.h"
#include "application/SizeService.h"
#include "application/StartupService.h"
#include "domain/model/EnabledAddons.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "infrastructure/sim/StartupFileLocations.h"
#include "infrastructure/sim/ExeXmlStartupEntries.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonChartCatalogueParser.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/documents/QtPdfChartVersions.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/fileops/WindowsSidecarStore.h"
#include "infrastructure/id/UuidLibraryIdGenerator.h"
#include "infrastructure/journal/JournalImportedFolders.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/settings/JsonSettingsRepository.h"
#include "infrastructure/sim/ContentListLocations.h"
#include "infrastructure/sim/ProfilePackages.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "infrastructure/sim/WindowsUserCfgLocations.h"
#include "shared/DisposableState.h"
#include "support/PathText.h"

#include "AppScroll.h"
#include "LibraryScroll.h"
#include "JournalScroll.h"
#include "SessionForMeasuring.h"
#include "application/Session.h"
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/CommunityModel.h"
#include "application/CoverageService.h"
#include "application/DocumentService.h"
#include "application/SceneryService.h"
#include "infrastructure/scenery/BglSceneryParser.h"
#include "infrastructure/scenery/JsonSceneryCache.h"
#include "infrastructure/sim/ContentXmlPackageList.h"
#include "view/library/AddonTreePage.h"
#include "viewmodel/CoverageViewModel.h"
#include "view/community/CommunityPage.h"
#include "view/JournalPage.h"
#include "view/shell/MainWindow.h"
#include "view/shell/PageNames.h"
#include "view/theme/PageTab.h"
#include "view/quarantine/QuarantinePage.h"
#include "viewmodel/AddonTreeViewModel.h"
#include "viewmodel/DeletionViewModel.h"
#include "viewmodel/AddonDocumentsViewModel.h"
#include "viewmodel/CommunityViewModel.h"
#include "viewmodel/ImportViewModel.h"
#include "viewmodel/JournalViewModel.h"
#include "viewmodel/QtBackgroundRunner.h"
#include "viewmodel/QuarantineViewModel.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    constexpr qint64 kBudgetForTheMainThread = 300;

    QTextStream& Out()
    {
        static QTextStream stream(stdout);
        return stream;
    }

    struct Measurement
    {
        QString stage;
        bool onTheMainThread = false;
        qint64 elapsed = 0;
    };

    std::vector<Measurement> measurements;

    template<typename Work>
    void Measure(const QString& stage, const bool onTheMainThread, Work&& work)
    {
        QElapsedTimer timer;
        timer.start();

        std::forward<Work>(work)();

        measurements.push_back({.stage = stage, .onTheMainThread = onTheMainThread, .elapsed = timer.elapsed()});
    }

    const SimulatorProfile* ActiveProfile(const AppSettings& settings)
    {
        const auto match = std::ranges::find_if(settings.profiles,
                                                [&settings](const SimulatorProfile& profile)
                                                {
                                                    return profile.id == settings.activeProfileId;
                                                });

        if (match != settings.profiles.end())
        {
            return &*match;
        }

        return settings.profiles.empty() ? nullptr : &settings.profiles.front();
    }

    qint64 MainThreadTotal()
    {
        qint64 total = 0;
        for (const Measurement& measurement : measurements)
        {
            total += measurement.onTheMainThread ? measurement.elapsed : 0;
        }

        return total;
    }

    void Report()
    {
        Out() << "\n"
              << QStringLiteral("stage").leftJustified(34) << QStringLiteral("thread").leftJustified(10) << "ms\n";

        for (const Measurement& measurement : measurements)
        {
            Out() << measurement.stage.leftJustified(34)
                  << QString(measurement.onTheMainThread ? "main" : "worker").leftJustified(10) << measurement.elapsed
                  << "\n";
        }

        Out() << "\ntotal on the main thread: " << MainThreadTotal() << " ms (budget " << kBudgetForTheMainThread
              << " ms)\n";
        Out() << (MainThreadTotal() > kBudgetForTheMainThread ? "RED: the interface freezes\n" : "GREEN\n");
        Out().flush();
    }
}

int main(int argc, char* argv[])
{
    const QApplication application(argc, argv);
    if (!QCoreApplication::arguments().contains(QStringLiteral("-style")))
    {
        QApplication::setStyle(QStringLiteral("windows11"));
    }

    const std::optional<DisposableState> staged = StageStateWhereWritingIsHarmless("fsorg-timing");
    if (!staged.has_value())
    {
        Out() << "could not stage a disposable copy of the state, so nothing ran\n";
        Out().flush();
        return 2;
    }

    Out() << "measuring a copy, so your install is never written: " << AsText(staged->settingsFile.parent_path())
          << "\n";

    JsonSettingsRepository settings(staged->settingsFile);
    JsonlOperationJournal journal(staged->journalFile);

    const AppSettings loaded = settings.Load().value_or(AppSettings{});
    const SimulatorProfile* active = ActiveProfile(loaded);

    if (active == nullptr)
    {
        Out() << "no profile configured in " << AsText(staged->settingsFile) << "\n";
        Out().flush();
        return 2;
    }

    const SimulatorProfile profile = *active;

    WindowsLinkService linkService;
    const WindowsFilesystemProbe filesystemProbe;
    WindowsFileOperations files;
    WindowsSidecarStore sidecars;
    const UuidLibraryIdGenerator identities;
    const JsonManifestParser manifestParser;
    const JournalImportedFolders importedFolders(journal);
    const FilesystemScanner catalog(manifestParser, filesystemProbe, importedFolders);
    const WindowsProcessProbe processProbe({"FlightSimulator.exe", "FlightSimulator2024.exe"});
    const SystemClock clock;

    const LinkingEngine linking(linkService, filesystemProbe);
    const EntryClassifier classifier(linkService, filesystemProbe);
    const OperationLog log(journal, clock);

    ExeXmlStartupEntries startupEntries{{}};
    StartupService startupService(startupEntries, processProbe, filesystemProbe, false);

    ProfileService profileService(catalog, filesystemProbe, sidecars, classifier, linking, log, identities,
                                  startupService, LinkType::Junction);

    const ImportEngine importEngine(filesystemProbe, files, sidecars, linking, log, LinkType::Junction,
                                    Verification::ByStructure);
    const ImportService importService(importEngine, processProbe, filesystemProbe, catalog, files, sidecars, linking,
                                      log, LinkType::Junction);
    const LibraryOrganizer organizer(catalog, filesystemProbe, files, linking, classifier, processProbe, log,
                                     LinkType::Junction);

    if (QCoreApplication::arguments().contains(QStringLiteral("--journal-scroll")))
    {
        const NoLibrariesToScan nothingToScan;
        ProfileService justTheProfile(nothingToScan, filesystemProbe, sidecars, classifier, linking, log, identities,
                                      startupService, LinkType::Junction);
        OneProfileRepository onlySettings(profile);
        InlineRunner runInline;
        SilentObserver silent;
        Session session(justTheProfile, organizer, onlySettings, onlySettings.Stored(), processProbe, runInline,
                        silent);
        session.ShowActiveProfile();

        return MeasureTheJournalScroll(journal, session);
    }

    const bool measuringTheJournal = QCoreApplication::arguments().contains(QStringLiteral("--app-journal"));
    const bool measuringTheLibrary = QCoreApplication::arguments().contains(QStringLiteral("--app-library"));

    if (measuringTheJournal || measuringTheLibrary)
    {
        MainWindow window(loaded);
        QtBackgroundRunner runner;
        TimedRunner timedRunner(runner);
        SessionNotifier notifier;
        Session session(profileService, organizer, settings, loaded, processProbe, timedRunner, notifier);

        SizeService sizes(catalog, filesystemProbe, clock, runner);

        AddonTreeModel treeModel;
        ProfilePackages packages(filesystemProbe, ContentListLocations(WindowsUserCfgLocations(), filesystemProbe));
        packages.Reload(session.Profile().variant);
        AddonTreeViewModel treeViewModel(session, profileService, treeModel, packages, sizes, runner, notifier);
        const DeletionService deletionService(filesystemProbe, files, sidecars, linking, classifier, processProbe, log,
                                              sizes);
        DeletionViewModel deletionViewModel(session, profileService, deletionService, sizes, runner);
        ImportViewModel importViewModel(importService, profileService, processProbe, session, runner);

        const std::vector<UserCfgLocation> userCfgLocations = WindowsUserCfgLocations();
        const std::vector<ContentListLocation> contentLists = ContentListLocations(userCfgLocations, filesystemProbe);
        const std::optional<ChosenContentList> chosen = ChooseContentList(contentLists, profile.variant);

        ContentXmlPackageList packageList{{}};
        packageList.Use(chosen.has_value() ? chosen->listPath : std::filesystem::path{});
        CoverageService coverageService(packageList, processProbe, loaded.managePackageList);
        const BglSceneryParser sceneryParser;
        JsonSceneryCache sceneryCache(QDir::tempPath().toStdString() + "/fsorg-timing-scenery-cache.json");
        SceneryService sceneryService(filesystemProbe, sceneryParser, clock, sceneryCache);
        CoverageViewModel coverageViewModel(coverageService, sceneryService, session, clock, runner);

        const JsonChartCatalogueParser catalogueParser;
        const QtPdfChartVersions chartVersions;
        const DocumentService documentService(filesystemProbe, catalogueParser, chartVersions);
        AddonDocumentsViewModel addonDocumentsViewModel(documentService, sceneryService, session, runner);

        auto* treePage = new AddonTreePage(treeViewModel, deletionViewModel, importViewModel, coverageViewModel,
                                           addonDocumentsViewModel, treeModel, notifier);

        CommunityModel communityModel;
        CommunityViewModel communityViewModel(profileService, session, notifier, communityModel, sizes);
        auto* communityPage = new CommunityPage(communityViewModel, importViewModel, communityModel);

        QuarantineModel quarantineModel;
        QuarantineViewModel quarantineViewModel(importService, profileService, session, notifier, quarantineModel,
                                                sizes, runner);
        auto* quarantinePage = new QuarantinePage(quarantineViewModel, quarantineModel);

        JournalModel journalModel;
        JournalViewModel journalViewModel(journal, session, journalModel);
        auto* journalPage = new JournalPage(journalViewModel, journalModel);

        PageTab* libraryTab = window.AddPage(PageNames::kLibrary, treePage);
        window.AddPage(PageNames::kDestinations, communityPage);
        window.AddPage(PageNames::kQuarantine, quarantinePage);
        window.AddPage(PageNames::kJournal, journalPage);

        treeViewModel.ShowActiveProfile();
        for (int pass = 0; pass < 400 && session.Snapshot().entries.empty(); ++pass)
        {
            QApplication::processEvents();
            QThread::msleep(5);
        }

        if (measuringTheLibrary)
        {
            libraryTab->click();

            return MeasureTheAppLibrary(window, *treePage, treeModel, coverageViewModel, sceneryService, session,
                                        timedRunner);
        }

        return MeasureTheAppJournal(window, *journalPage, journalViewModel, journalModel);
    }

    AddonTreeModel model;
    CommunityModel communityModel;
    InlineRunner runInline;
    SessionNotifier notifier;
    Session session(profileService, organizer, settings, loaded, processProbe, runInline, notifier);
    SizeService inlineSizes(catalog, filesystemProbe, clock, runInline);
    CommunityViewModel communityViewModel(profileService, session, notifier, communityModel, inlineSizes);

    Measure("Session::ShowActiveProfile", false,
            [&]
            {
                session.ShowActiveProfile();
            });

    Measure("AddonTreeModel::Show", true,
            [&]
            {
                model.Show(session.Snapshot(), session.Profile());
            });

    if (QCoreApplication::arguments().contains(QStringLiteral("--toggle")))
    {
        startupEntries.Use(
            StartupFileOf(StartupFileLocations(WindowsUserCfgLocations(), filesystemProbe), profile.variant));
        session.RefreshStartupEntries();
        measurements.clear();

        std::vector<const TreeNode*> everyAddon;
        for (const TreeNode& library : session.Snapshot().libraries)
        {
            std::ranges::copy(AddonsUnder(library), std::back_inserter(everyAddon));
        }

        const EnabledAddons enabledNow = session.Snapshot().enabled;
        const auto enabledOne = std::ranges::find_if(everyAddon,
                                                     [&enabledNow](const TreeNode* addon)
                                                     {
                                                         return enabledNow.Contains(addon->path);
                                                     });
        const auto disabledOne = std::ranges::find_if(everyAddon,
                                                      [&enabledNow](const TreeNode* addon)
                                                      {
                                                          return !enabledNow.Contains(addon->path);
                                                      });

        if (enabledOne == everyAddon.end() || disabledOne == everyAddon.end())
        {
            Out() << "needs one enabled and one disabled addon\n";
            Out().flush();
            return 2;
        }

        const std::vector<const TreeNode*> turningOff{*enabledOne};
        const std::vector<const TreeNode*> turningOn{*disabledOne};

        const std::vector<DestinationEntry>& shownEntries = session.Snapshot().entries;
        const auto itsLink =
            std::ranges::find_if(shownEntries,
                                 [&enabledOne](const DestinationEntry& entry)
                                 {
                                     return ComparablePath(entry.target) == ComparablePath((*enabledOne)->path);
                                 });
        const std::vector<std::filesystem::path> changed = itsLink == shownEntries.end()
            ? std::vector<std::filesystem::path>{}
            : std::vector<std::filesystem::path>{itsLink->path};

        ProfilePackages packages(filesystemProbe, ContentListLocations(WindowsUserCfgLocations(), filesystemProbe));
        packages.Reload(session.Profile().variant);
        AddonTreeViewModel treeViewModel(session, profileService, model, packages, inlineSizes, runInline, notifier);

        bool theIncrementalListMatches = true;

        for (int round = 1; round <= 3; ++round)
        {
            const QString tag = QStringLiteral("r%1 ").arg(round);

            Measure(tag + "PlanToggle (enable)", true,
                    [&]
                    {
                        static_cast<void>(treeViewModel.PlanToggle(turningOn, true));
                    });
            Measure(tag + "StartupEntriesAtRisk (disable)", true,
                    [&]
                    {
                        static_cast<void>(treeViewModel.StartupEntriesAtRisk(turningOff));
                    });
            Measure(tag + "trees copied for the worker", true,
                    [&]
                    {
                        const std::vector<TreeNode> copied = session.Snapshot().libraries;
                        static_cast<void>(copied.size());
                    });

            std::vector<ExternalAddon> externals;
            ProfileService::LinksOnDisk onDisk;
            std::vector<DestinationEntry> incremental;

            Measure(tag + "externals (sidecars)", false,
                    [&]
                    {
                        externals =
                            profileService.WhatCameFromAnotherProgram(session.Profile(), session.Snapshot().libraries);
                    });
            Measure(tag + "ReadLinksNow", false,
                    [&]
                    {
                        onDisk = profileService.ReadLinksNow(session.Profile(), externals);
                    });

            Measure(
                tag + "EntriesAfter (incremental)", false,
                [&]
                {
                    incremental = profileService.EntriesAfter(
                        session.Profile(), onDisk.entries,
                        {LinkOperationResult{.linkPath = changed.empty() ? std::filesystem::path{} : changed.front()}},
                        externals);
                });
            Measure(tag + "SimulatorIsRunning", false,
                    [&]
                    {
                        static_cast<void>(processProbe.SimulatorIsRunning());
                    });

            const std::vector<DestinationEntry> full =
                profileService.ResolveEntries(session.Profile(), session.Snapshot().libraries);

            theIncrementalListMatches = theIncrementalListMatches && incremental.size() == full.size()
                && std::ranges::equal(incremental, full,
                                      [](const DestinationEntry& left, const DestinationEntry& right)
                                      {
                                          return left.path == right.path && left.target == right.target
                                              && left.classification == right.classification;
                                      });

            EntriesRead read;

            Measure(tag + "ReadEntries (resolve and derive)", false,
                    [&]
                    {
                        read =
                            profileService.ReadEntries(session.StampForAnEntriesRead(), session.Snapshot().libraries);
                    });
            Measure(tag + "Session::AdoptTheEntriesRead", true,
                    [&]
                    {
                        session.AdoptTheEntriesRead(std::move(read));
                    });
            Measure(tag + "AddonTreeModel::Refresh", true,
                    [&]
                    {
                        model.Refresh(session.Snapshot(), session.Profile());
                    });
        }

        Out() << "addons: " << everyAddon.size() << "  entries: " << session.Snapshot().entries.size()
              << "  startup entries: " << session.Snapshot().startupEntries.size() << "\n";
        for (const EntryClassification classification : kEveryClassification)
        {
            Out() << "  classification #" << OrderOf(classification) << ": "
                  << std::ranges::count(session.Snapshot().entries, classification, &DestinationEntry::classification)
                  << "\n";
        }
        Out() << "incremental list equals a full read of the same disk: " << (theIncrementalListMatches ? "yes" : "NO")
              << "\n";

        Report();

        return theIncrementalListMatches ? 0 : 1;
    }

    Measure("CommunityViewModel::Show", true,
            [&]
            {
                communityViewModel.Show();
            });
    Measure("ImportService::Leftovers", false,
            [&]
            {
                static_cast<void>(importService.Leftovers(profile));
            });
    Measure("ImportService::Quarantined", true,
            [&]
            {
                static_cast<void>(importService.Quarantined(profile));
            });

    Out() << "profile: " << QString::fromStdString(profile.id) << "  libraries: " << profile.libraries.size()
          << "  entries: " << session.Snapshot().entries.size() << "\n";

    Report();

    return MainThreadTotal() > kBudgetForTheMainThread ? 1 : 0;
}
