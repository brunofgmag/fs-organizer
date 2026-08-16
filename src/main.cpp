#include <optional>

#include <QtCore/QDir>
#include <QtCore/QLibraryInfo>
#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include "application/CoverageService.h"
#include "application/DeletionService.h"
#include "application/BisectionService.h"
#include "application/ImportService.h"
#include "application/LegacyConfigImporter.h"
#include "application/LibraryOrganizer.h"
#include "application/PresetService.h"
#include "application/ProfileService.h"
#include "application/Session.h"
#include "application/SceneryService.h"
#include "application/SetupService.h"
#include "application/DocumentService.h"
#include "application/SizeService.h"
#include "application/StartupService.h"
#include "infrastructure/bisection/JsonBisectionStore.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonChartCatalogueParser.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/documents/JsonDocumentIndexCache.h"
#include "infrastructure/documents/QtPdfChartVersions.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/fileops/WindowsSidecarStore.h"
#include "infrastructure/id/UuidLibraryIdGenerator.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/legacy/WindowsLegacyConfigSource.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SingleInstance.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/preset/FilePresetRepository.h"
#include "infrastructure/scenery/BglSceneryParser.h"
#include "infrastructure/scenery/JsonSceneryCache.h"
#include "infrastructure/settings/JsonSettingsRepository.h"
#include "infrastructure/sim/ContentListLocations.h"
#include "infrastructure/sim/ContentXmlPackageList.h"
#include "infrastructure/sim/ExeXmlStartupEntries.h"
#include "infrastructure/sim/LoadingReportLocations.h"
#include "infrastructure/sim/ProfileLoadingReport.h"
#include "infrastructure/sim/ProfilePackages.h"
#include "infrastructure/sim/StartupFileLocations.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "infrastructure/sim/WindowsSimulatorLocator.h"
#include "infrastructure/sim/WindowsUserCfgLocations.h"
#include "infrastructure/update/GithubUpdateService.h"
#include "support/PathText.h"
#include "view/library/AddonTreePage.h"
#include "view/community/CommunityPage.h"
#include "view/diagnostics/DiagnosticsPage.h"
#include "view/documents/DocumentsPage.h"
#include "view/legacy/LegacyImportDialog.h"
#include "view/JournalPage.h"
#include "view/shell/LanguageSwitch.h"
#include "view/options/OptionsPage.h"
#include "view/shell/LongOperationProgress.h"
#include "view/shell/MainWindow.h"
#include "view/shell/PageNames.h"
#include "view/shell/StartupOffers.h"
#include "view/PresetsPage.h"
#include "view/quarantine/QuarantinePage.h"
#include "view/setup/SetupWizard.h"
#include "view/simulator/PackageListPage.h"
#include "view/simulator/SimulatorPage.h"
#include "view/simulator/StartupPage.h"
#include "view/theme/ModernistTheme.h"
#include "view/theme/PageTab.h"
#include "viewmodel/CommunityViewModel.h"
#include "viewmodel/CoverageViewModel.h"
#include "viewmodel/DeletionViewModel.h"
#include "viewmodel/AddonDocumentsViewModel.h"
#include "viewmodel/DocumentsViewModel.h"
#include "viewmodel/BisectionViewModel.h"
#include "viewmodel/DiagnosticsViewModel.h"
#include "viewmodel/ImportViewModel.h"
#include "viewmodel/JournalViewModel.h"
#include "viewmodel/LegacyImportViewModel.h"
#include "viewmodel/OptionsViewModel.h"
#include "viewmodel/QtBackgroundRunner.h"
#include "viewmodel/QuarantineViewModel.h"
#include "viewmodel/SessionNotifier.h"
#include "viewmodel/SetupViewModel.h"
#include "viewmodel/StartupViewModel.h"
#include "viewmodel/UpdateViewModel.h"

namespace
{
    constexpr auto kSingleInstanceName = L"Local\\fs-organizer-single-instance";
    constexpr auto kDefaultUpdateFeed = "https://api.github.com/repos/brunofgmag/fs-organizer/releases/latest";
    constexpr int kFirstUpdateCheckDelayMs = 3000;

    UpdateOffer WhatToOffer(const UpdateState state)
    {
        if (state == UpdateState::ReadyToApply)
        {
            return UpdateOffer::Staged;
        }

        if (state == UpdateState::Available || state == UpdateState::Downloading)
        {
            return UpdateOffer::Available;
        }

        return UpdateOffer::None;
    }

    bool UpdatesAreOn()
    {
        if (qEnvironmentVariableIsSet("FSORG_NO_UPDATES"))
        {
            return false;
        }

        QDir folder(QCoreApplication::applicationDirPath());
        for (int level = 0; level < 4; ++level)
        {
            if (folder.exists(QStringLiteral("CMakeCache.txt")))
            {
                return false;
            }

            if (!folder.cdUp())
            {
                break;
            }
        }

        return true;
    }

    KeepTheProfile WriteItTo(SettingsRepository& repository, AppSettings& stored)
    {
        return [&repository, &stored](const SimulatorProfile& profile)
        {
            AppSettings next = stored;
            AddProfile(next, profile);

            if (!repository.Save(next))
            {
                return false;
            }

            stored = std::move(next);

            return true;
        };
    }

    KeepTheProfile AddItTo(Session& session)
    {
        return [&session](const SimulatorProfile& profile)
        {
            return session.Rewrite(
                [&profile](AppSettings& settings)
                {
                    AddProfile(settings, profile);

                    return true;
                });
        };
    }

    bool RunSetup(std::vector<SimulatorProfile> existing,
                  KeepTheProfile keep,
                  const SimulatorLocator& locator,
                  const FilesystemProbe& filesystemProbe,
                  const LibraryIdGenerator& identities,
                  const CatalogScanner& catalog)
    {
        SetupService service(locator, filesystemProbe, identities, catalog, std::move(existing), std::move(keep));
        SetupViewModel viewModel(service);
        viewModel.Detect();

        SetupWizard wizard(viewModel);

        return wizard.exec() == QDialog::Accepted;
    }

}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("FS Organizer"));
    QApplication::setApplicationVersion(QStringLiteral(FSORG_VERSION));
    QApplication::setOrganizationName(QStringLiteral("fs-organizer"));
    QApplication::setWindowIcon(BrandIcon());

    const SingleInstance onlyOne(kSingleInstanceName);
    if (onlyOne.AnotherIsRunning())
    {
        BringTheRunningInstanceForward(QApplication::applicationName().toStdWString());

        return 0;
    }

    ApplyModernistTheme(app);

    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &app,
                     [&app]
                     {
                         RefreshModernistTheme(app);
                     });

    LanguageSwitch language;
    static_cast<void>(language.Use({}));

    WindowsLinkService linkService;
    const WindowsFilesystemProbe filesystemProbe;
    WindowsFileOperations files;
    WindowsSidecarStore sidecars;
    const UuidLibraryIdGenerator identities;
    const JsonManifestParser manifestParser;
    const FilesystemScanner catalog(manifestParser, filesystemProbe);
    const std::vector<UserCfgLocation> userCfgLocations = WindowsUserCfgLocations();
    const WindowsSimulatorLocator locator(userCfgLocations);
    ProfilePackages packages(filesystemProbe, ContentListLocations(userCfgLocations, filesystemProbe));
    const WindowsProcessProbe processProbe({"FlightSimulator.exe", "FlightSimulator2024.exe"});
    const SystemClock clock;
    JsonSettingsRepository settings(SettingsFilePath());
    JsonlOperationJournal journal(JournalFilePath());

    const std::optional<AppSettings> loaded = settings.Load();
    if (!loaded.has_value())
    {
        QMessageBox::critical(nullptr, QObject::tr("Unreadable configuration"),
                              QObject::tr("The configuration file exists but could not be read, so FS Organizer will "
                                          "not overwrite it. Move or fix %1 and open the program again.")
                                  .arg(AsText(SettingsFilePath())));

        return 1;
    }

    AppSettings stored = *loaded;

    static_cast<void>(language.Use(QString::fromStdString(stored.language)));

    const bool setupJustRan = stored.profiles.empty();
    if (setupJustRan
        && !RunSetup(stored.profiles, WriteItTo(settings, stored), locator, filesystemProbe, identities, catalog))
    {
        return 0;
    }

    const LinkingEngine linking(linkService, filesystemProbe);
    const EntryClassifier classifier(linkService, filesystemProbe);
    const OperationLog log(journal, clock);

    const LinkType storedLinkType = stored.linkType;
    const Verification storedVerification = stored.verification;

    const std::vector<StartupFileLocation> startupFiles = StartupFileLocations(userCfgLocations, filesystemProbe);
    ExeXmlStartupEntries startupEntries{{}};
    StartupService startupService(startupEntries, processProbe, filesystemProbe, stored.manageStartupEntries);

    const std::vector<LoadingReportLocation> loadingReports = LoadingReportLocations(userCfgLocations, filesystemProbe);
    ProfileLoadingReport loadingReport(filesystemProbe, {});

    const std::vector<ContentListLocation> contentLists = ContentListLocations(userCfgLocations, filesystemProbe);
    ContentXmlPackageList packageList{{}};
    CoverageService coverageService(packageList, processProbe, stored.managePackageList);

    const BglSceneryParser sceneryParser;
    JsonSceneryCache sceneryCache(SceneryCacheFilePath());

    ProfileService profileService(catalog, filesystemProbe, sidecars, classifier, linking, log, identities,
                                  startupService, storedLinkType);

    ImportEngine importEngine(filesystemProbe, files, sidecars, linking, log, storedLinkType, storedVerification);
    ImportService importService(importEngine, processProbe, filesystemProbe, catalog, files, sidecars, linking, log,
                                storedLinkType);

    LibraryOrganizer organizer(catalog, filesystemProbe, files, linking, classifier, processProbe, log, storedLinkType);

    MainWindow window(stored);
    QtBackgroundRunner runner;
    SessionNotifier notifier;
    Session session(profileService, organizer, settings, stored, processProbe, runner, notifier);

    SizeService sizes(catalog, filesystemProbe, clock, runner);
    SceneryService sceneryService(filesystemProbe, sceneryParser, clock, sceneryCache);

    CoverageViewModel coverageViewModel(coverageService, sceneryService, session, clock);

    AddonTreeModel model;
    AddonTreeViewModel treeViewModel(session, profileService, model, packages, sizes, notifier);

    const DeletionService deletionService(filesystemProbe, files, sidecars, linking, classifier, processProbe, log,
                                          sizes);
    DeletionViewModel deletionViewModel(session, profileService, deletionService, sizes);

    QObject::connect(&notifier, &SessionNotifier::ScanFinished, &window,
                     [&packages, &session]
                     {
                         packages.Reload(session.Profile().variant);
                     });
    ImportViewModel importViewModel(importService, profileService, processProbe, session, runner);

    const JsonChartCatalogueParser catalogueParser;
    const QtPdfChartVersions chartVersions;
    const DocumentService documentService(catalog, filesystemProbe, catalogueParser, chartVersions);
    AddonDocumentsViewModel addonDocumentsViewModel(documentService, sceneryService, session, runner);
    JsonDocumentIndexCache documentIndexCache(DocumentIndexFilePath());
    DocumentsViewModel documentsViewModel(documentService, sceneryService, session, runner, documentIndexCache, clock);

    auto* page = new AddonTreePage(treeViewModel, deletionViewModel, importViewModel, coverageViewModel,
                                   addonDocumentsViewModel, model, notifier);

    documentsViewModel.ShowWhatWasKept();

    auto* documentsPage = new DocumentsPage(documentsViewModel);

    QObject::connect(&notifier, &SessionNotifier::ScanFinished, documentsPage,
                     [&documentsViewModel]
                     {
                         documentsViewModel.ReadTheLibrary();
                     });

    LongOperationProgress progress(importViewModel, &window);

    CommunityModel communityModel;
    CommunityViewModel communityViewModel(profileService, session, notifier, communityModel, sizes);
    auto* communityPage = new CommunityPage(communityViewModel, importViewModel, communityModel);

    QuarantineModel quarantineModel;
    QuarantineViewModel quarantineViewModel(importService, profileService, session, notifier, quarantineModel, sizes,
                                            runner);
    auto* quarantinePage = new QuarantinePage(quarantineViewModel, quarantineModel);

    JournalModel journalModel;
    JournalViewModel journalViewModel(journal, session, journalModel);
    auto* journalPage = new JournalPage(journalViewModel, journalModel);

    DiagnosticsViewModel diagnosticsViewModel(importService, sizes, sceneryService, session, loadingReport, clock,
                                              runner);

    const CouplingScan coupling(filesystemProbe);
    JsonBisectionStore bisectionStore(BisectionFolderPath());
    BisectionService bisectionService(profileService, coupling, filesystemProbe, bisectionStore, clock);
    BisectionViewModel bisectionViewModel(bisectionService, session);

    auto* diagnosticsPage = new DiagnosticsPage(diagnosticsViewModel, bisectionViewModel);

    StartupViewModel startupViewModel(startupService, session, clock);
    auto* startupPage = new StartupPage(startupViewModel);
    auto* packageListPage = new PackageListPage(coverageViewModel);
    auto* simulatorPage = new SimulatorPage(startupPage, packageListPage);

    QObject::connect(&notifier, &SessionNotifier::ScanFinished, startupPage,
                     [&session, &startupEntries, &startupFiles, &startupViewModel, &packageList, &contentLists,
                      &coverageViewModel, &loadingReport, &loadingReports]
                     {
                         startupEntries.Use(StartupFileOf(startupFiles, session.Profile().variant));
                         loadingReport.Use(LoadingReportOf(loadingReports, session.Profile().variant));
                         startupViewModel.Show();
                         session.RefreshStartupEntries();

                         const std::optional<ChosenContentList> chosen =
                             ChooseContentList(contentLists, session.Profile().variant);
                         packageList.Use(chosen.has_value() ? chosen->listPath : std::filesystem::path{});
                         coverageViewModel.Show();
                     });

    FilePresetRepository presetRepository(PresetsFolderPath());
    PresetService presetService(presetRepository, profileService, startupService);
    PresetViewModel presetViewModel(session, presetService, profileService);
    auto* presetsPage = new PresetsPage(presetViewModel, notifier);

    GithubUpdateService updateService(
        qEnvironmentVariable("FSORG_UPDATE_FEED", QString::fromLatin1(kDefaultUpdateFeed)),
        QCoreApplication::applicationVersion(), GithubUpdateService::DefaultUpdatesFolder());
    updateService.DiscardStaged();

    UpdateViewModel updateViewModel(updateService, stored.updateMode, UpdatesAreOn());

    OptionsViewModel optionsViewModel(session, profileService, notifier);
    auto* optionsPage = new OptionsPage(optionsViewModel, updateViewModel, SettingsFilePath());

    const WindowsLegacyConfigSource legacyConfig(ProgramDataFolder());
    const LegacyConfigImporter legacyImporter(legacyConfig, filesystemProbe);
    LegacyImportViewModel legacyViewModel(session, legacyImporter, presetService);

    PageTab* libraryButton = window.AddPage(PageNames::kLibrary, page);
    PageTab* communityButton = window.AddPage(PageNames::kDestinations, communityPage);
    window.AddPage(PageNames::kSimulator, simulatorPage);
    PageTab* presetsButton = window.AddPage(PageNames::kPresets, presetsPage);
    PageTab* documentsButton = window.AddPage(PageNames::kDocuments, documentsPage);
    PageTab* quarantineButton = window.AddPage(PageNames::kQuarantine, quarantinePage);
    window.AddPage(PageNames::kDiagnostics, diagnosticsPage);
    window.AddPage(PageNames::kJournal, journalPage);

    window.CarryOptionsOn(optionsPage);
    window.CarryTriageOn(page);
    window.CarryTriageOn(communityPage);

    QObject::connect(&optionsViewModel, &OptionsViewModel::LinkTypeChosen, &window,
                     [&importEngine, &importService, &organizer](const LinkType linkType)
                     {
                         importEngine.UseLinkType(linkType);
                         importService.UseLinkType(linkType);
                         organizer.UseLinkType(linkType);
                     });

    QObject::connect(&optionsViewModel, &OptionsViewModel::VerificationChosen, &window,
                     [&importEngine](const Verification verification)
                     {
                         importEngine.UseVerification(verification);
                     });

    QObject::connect(&optionsViewModel, &OptionsViewModel::LanguageChosen, &window,
                     [&language, &window](const QString& chosen)
                     {
                         if (!language.Use(chosen))
                         {
                             QMessageBox::warning(&window, QObject::tr("Language not applied"),
                                                  QObject::tr("The translation for %1 did not load, so the interface "
                                                              "stays in English. The choice was still written down.")
                                                      .arg(chosen));
                         }
                     });

    QObject::connect(&optionsViewModel, &OptionsViewModel::SettingsCouldNotBeSaved, &window,
                     [&window]
                     {
                         QMessageBox::warning(
                             &window, QObject::tr("Could not save"),
                             QObject::tr("The option could not be written to %1, so it stays as it was.")
                                 .arg(AsText(SettingsFilePath())));
                     });

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &updateViewModel,
                     [&updateViewModel, &updateService]
                     {
                         if (updateViewModel.ShouldApplyOnExit())
                         {
                             static_cast<void>(updateService.LaunchApplyHelper(false));
                         }
                     });

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &treeViewModel, &AddonTreeViewModel::CancelScan);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &documentsViewModel, &DocumentsViewModel::Stop);

    QObject::connect(page, &AddonTreePage::DocumentationRequested, documentsPage,
                     [documentsButton, documentsPage](const std::string& addon)
                     {
                         documentsButton->click();
                         documentsPage->Reveal(addon);
                     });

    QTimer::singleShot(kFirstUpdateCheckDelayMs, &updateViewModel, &UpdateViewModel::CheckQuietly);

    const auto sayWhatTheUpdateIs = [&window, &updateViewModel]
    {
        window.ShowUpdateOffer(WhatToOffer(updateViewModel.State()), updateViewModel.OfferedVersion());
    };

    QObject::connect(&updateViewModel, &UpdateViewModel::Changed, &window, sayWhatTheUpdateIs);
    sayWhatTheUpdateIs();

    QObject::connect(&window, &MainWindow::UpdateOfferChosen, &window,
                     [&window, &updateViewModel, optionsPage]
                     {
                         if (updateViewModel.State() == UpdateState::ReadyToApply)
                         {
                             updateViewModel.ApplyAndRestart();
                             return;
                         }

                         optionsPage->Reload();
                         optionsPage->ShowTheUpdates();
                         window.ShowOptions();
                     });

    QObject::connect(&window, &MainWindow::OptionsRequested, optionsPage, &OptionsPage::Reload);
    QObject::connect(optionsPage, &OptionsPage::StatusChanged, &window, &MainWindow::ShowStatus);
    QObject::connect(optionsPage, &OptionsPage::SummaryChanged, &window,
                     [&window, optionsPage](const QString& summary)
                     {
                         window.ShowSummary(optionsPage, summary);
                     });
    QObject::connect(optionsPage, &OptionsPage::AddProfileRequested, &window, &MainWindow::AddProfileRequested);
    QObject::connect(optionsPage, &OptionsPage::LegacyImportRequested, &window,
                     [&window, &legacyViewModel, optionsPage, &presetViewModel]
                     {
                         LegacyImportDialog dialog(legacyViewModel, &window);
                         QObject::connect(&dialog, &LegacyImportDialog::StatusChanged, &window,
                                          &MainWindow::ShowStatus);

                         if (dialog.exec() == QDialog::Accepted)
                         {
                             optionsPage->Reload();
                             emit presetViewModel.Changed();
                         }
                     });
    QObject::connect(optionsPage, &OptionsPage::ProfileChosen, &window,
                     [&window](const std::string& profileId)
                     {
                         emit window.ProfileChosen(profileId);
                     });
    QObject::connect(&optionsViewModel, &OptionsViewModel::LinksDisabled, page,
                     [page](const std::vector<LinkOperationResult>&)
                     {
                         page->RefreshUndoState();
                     });

    const auto counted = [](const std::size_t count)
    {
        return count > 0 ? std::optional(static_cast<qsizetype>(count)) : std::nullopt;
    };
    QObject::connect(&model, &QAbstractItemModel::modelReset, libraryButton,
                     [libraryButton, &model, counted]
                     {
                         libraryButton->ShowCount(counted(model.AddonCount()));
                     });
    QObject::connect(&communityModel, &QAbstractItemModel::modelReset, communityButton,
                     [communityButton, &communityModel, counted]
                     {
                         communityButton->ShowCount(counted(static_cast<std::size_t>(communityModel.rowCount({}))));
                     });
    const auto showThePresetCount = [presetsButton, &presetViewModel, counted]
    {
        presetsButton->ShowCount(counted(static_cast<std::size_t>(presetViewModel.Names().size())));
    };
    QObject::connect(&presetViewModel, &PresetViewModel::Changed, presetsButton, showThePresetCount);
    showThePresetCount();
    QObject::connect(&quarantineModel, &QAbstractItemModel::modelReset, quarantineButton,
                     [quarantineButton, &quarantineModel, counted]
                     {
                         quarantineButton->ShowCount(counted(static_cast<std::size_t>(quarantineModel.rowCount({}))));
                     });

    const auto carryTheSummaryOf = [&window](QWidget* pg)
    {
        return [&window, pg](const QString& summary)
        {
            window.ShowSummary(pg, summary);
        };
    };

    const auto carryTheAsideOf = [&window](QWidget* pg)
    {
        return [&window, pg](const QString& aside)
        {
            window.ShowAside(pg, aside);
        };
    };
    QObject::connect(page, &AddonTreePage::SummaryChanged, &window, carryTheSummaryOf(page));
    QObject::connect(communityPage, &CommunityPage::SummaryChanged, &window, carryTheSummaryOf(communityPage));
    QObject::connect(quarantinePage, &QuarantinePage::SummaryChanged, &window, carryTheSummaryOf(quarantinePage));
    QObject::connect(journalPage, &JournalPage::SummaryChanged, &window, carryTheSummaryOf(journalPage));
    QObject::connect(presetsPage, &PresetsPage::SummaryChanged, &window, carryTheSummaryOf(presetsPage));

    QObject::connect(communityPage, &CommunityPage::AsideChanged, &window, carryTheAsideOf(communityPage));
    QObject::connect(quarantinePage, &QuarantinePage::AsideChanged, &window, carryTheAsideOf(quarantinePage));
    QObject::connect(journalPage, &JournalPage::AsideChanged, &window, carryTheAsideOf(journalPage));

    QObject::connect(page, &AddonTreePage::MeterChanged, &window,
                     [&window, page](const int filled, const int outOf)
                     {
                         window.ShowMeter(page, filled, outOf);
                     });

    QObject::connect(page, &AddonTreePage::StatusChanged, &window, &MainWindow::ShowStatus);
    QObject::connect(communityPage, &CommunityPage::StatusChanged, &window, &MainWindow::ShowStatus);
    QObject::connect(quarantinePage, &QuarantinePage::StatusChanged, &window, &MainWindow::ShowStatus);
    QObject::connect(presetsPage, &PresetsPage::StatusChanged, &window, &MainWindow::ShowStatus);

    QObject::connect(&communityViewModel, &CommunityViewModel::RepairFinished, page, &AddonTreePage::RefreshUndoState);

    QObject::connect(page, &AddonTreePage::ConflictChosen, communityPage, &CommunityPage::ResolveConflict);

    const auto adoptWhatChangedOnDisk = [page, &treeViewModel]
    {
        page->RefreshUndoState();
        treeViewModel.ShowActiveProfile();
    };

    QObject::connect(&importViewModel, &ImportViewModel::Finished, page,
                     [adoptWhatChangedOnDisk](const std::vector<ImportOperationResult>&)
                     {
                         adoptWhatChangedOnDisk();
                     });
    QObject::connect(&importViewModel, &ImportViewModel::ConflictsResolved, page,
                     [adoptWhatChangedOnDisk](const std::vector<FileOperationResult>&)
                     {
                         adoptWhatChangedOnDisk();
                     });
    QObject::connect(&importViewModel, &ImportViewModel::GaveBack, page,
                     [adoptWhatChangedOnDisk](const std::vector<FileOperationResult>&)
                     {
                         adoptWhatChangedOnDisk();
                     });
    QObject::connect(&quarantineViewModel, &QuarantineViewModel::Restored, page,
                     [adoptWhatChangedOnDisk](const std::vector<FileOperationResult>&)
                     {
                         adoptWhatChangedOnDisk();
                     });
    QObject::connect(&quarantineViewModel, &QuarantineViewModel::Swapped, page,
                     [adoptWhatChangedOnDisk](const std::vector<SwapResult>&)
                     {
                         adoptWhatChangedOnDisk();
                     });
    QObject::connect(&deletionViewModel, &DeletionViewModel::Deleted, page,
                     [adoptWhatChangedOnDisk](const std::vector<DeletionResult>&, const DeletionRoute)
                     {
                         adoptWhatChangedOnDisk();
                     });

    QObject::connect(&window, &MainWindow::PageSelected, &window,
                     [&](const QWidget* selected)
                     {
                         if (selected == communityPage)
                         {
                             communityViewModel.ReadTheDestinationsAgain();
                         }
                         else if (selected == quarantinePage)
                         {
                             quarantineViewModel.Show();
                         }
                         else if (selected == journalPage)
                         {
                             journalViewModel.Show();
                         }
                         else if (selected == diagnosticsPage)
                         {
                             diagnosticsViewModel.Show();
                         }
                         else if (selected == simulatorPage)
                         {
                             startupViewModel.Show();
                             coverageViewModel.Show();
                         }
                     });

    QObject::connect(diagnosticsPage, &DiagnosticsPage::SummaryChanged, &window, carryTheSummaryOf(diagnosticsPage));
    QObject::connect(diagnosticsPage, &DiagnosticsPage::StatusChanged, &window, &MainWindow::ShowStatus);
    QObject::connect(diagnosticsPage, &DiagnosticsPage::QuarantineRequested, quarantinePage,
                     [quarantineButton]
                     {
                         quarantineButton->click();
                     });
    QObject::connect(diagnosticsPage, &DiagnosticsPage::RepairRequested, &window, &MainWindow::RepairRequested);
    QObject::connect(diagnosticsPage, &DiagnosticsPage::ImportRequested, &window, &MainWindow::ImportRequested);

    QObject::connect(startupPage, &StartupPage::SummaryChanged, simulatorPage,
                     [simulatorPage, startupPage](const QString& summary)
                     {
                         simulatorPage->CarrySummaryFrom(startupPage, summary);
                     });
    QObject::connect(packageListPage, &PackageListPage::SummaryChanged, simulatorPage,
                     [simulatorPage, packageListPage](const QString& summary)
                     {
                         simulatorPage->CarrySummaryFrom(packageListPage, summary);
                     });
    QObject::connect(simulatorPage, &SimulatorPage::SummaryChanged, &window, carryTheSummaryOf(simulatorPage));
    QObject::connect(startupPage, &StartupPage::StatusChanged, &window, &MainWindow::ShowStatus);
    QObject::connect(packageListPage, &PackageListPage::StatusChanged, &window, &MainWindow::ShowStatus);

    QObject::connect(&communityViewModel, &CommunityViewModel::BreakdownChanged, &window,
                     [&window](const AttentionBreakdown& breakdown)
                     {
                         window.ShowTriage(breakdown);
                     });
    QObject::connect(&window, &MainWindow::RepairRequested, communityPage,
                     [communityButton, communityPage]
                     {
                         communityButton->click();
                         communityPage->StartRepair();
                     });
    QObject::connect(&window, &MainWindow::ResolveRequested, communityPage,
                     [communityButton, communityPage]
                     {
                         communityButton->click();
                         communityPage->FilterByConflicted();
                         communityPage->SelectEverythingShown();
                         communityPage->ResolveTheSelectedConflict();
                     });
    QObject::connect(&window, &MainWindow::DuplicatesRequested, communityPage,
                     [communityButton, communityPage]
                     {
                         communityButton->click();
                         communityPage->FilterBy(EntryClassification::Duplicated);
                     });
    QObject::connect(&window, &MainWindow::ImportRequested, communityPage,
                     [communityButton, communityPage]
                     {
                         communityButton->click();
                         communityPage->FilterBy(EntryClassification::Unmanaged);
                         communityPage->SelectEverythingShown();
                         communityPage->StartImport();
                     });

    QObject::connect(&notifier, &SessionNotifier::RestartPendingChanged, &window, &MainWindow::ShowRestartPending);
    QObject::connect(&notifier, &SessionNotifier::SimulatorIsRunning, &window, &MainWindow::WarnTheSimulatorIsOpen);
    QObject::connect(&presetViewModel, &PresetViewModel::Applied, page,
                     [page](const QStringList&)
                     {
                         page->RefreshUndoState();
                     });
    QObject::connect(&window, &MainWindow::ProfileChosen, &treeViewModel, &AddonTreeViewModel::ChooseProfile);

    QObject::connect(
        &window, &MainWindow::AddProfileRequested, &window,
        [&]
        {
            if (RunSetup(session.Settings().profiles, AddItTo(session), locator, filesystemProbe, identities, catalog))
            {
                window.ShowProfiles(session.Settings());
                treeViewModel.ShowActiveProfile();
            }
        });

    QObject::connect(&notifier, &SessionNotifier::ScanFinished, &window,
                     [&window, &session]
                     {
                         window.ShowProfiles(session.Settings());
                     });

    QObject::connect(&notifier, &SessionNotifier::SettingsCouldNotBeSaved, &window,
                     [&]
                     {
                         window.ShowProfiles(session.Settings());
                         optionsPage->Reload();

                         QMessageBox::warning(
                             &window, QObject::tr("Could not save"),
                             QObject::tr("The change was applied on the disk, but the profile could not be written to "
                                         "%1. Next time the program opens it will not be recorded.")
                                 .arg(AsText(SettingsFilePath())));
                     });

    QObject::connect(&notifier, &SessionNotifier::ScanFinished, &window,
                     [&, once = false]() mutable
                     {
                         if (once)
                         {
                             return;
                         }

                         once = true;
                         OfferToDropTheOverridesThatPointNowhere(session, &window);
                         OfferToPutBackWhatALostSwapRenamed(importViewModel, &window);
                         OfferWhatALostImportLeftBehind(importViewModel, &window);
                         OfferToCarryOnTheSearchThatWasLeftHalfway(bisectionViewModel, &window);

                         if (setupJustRan)
                         {
                             OfferWhatTheOldProgramKept(legacyViewModel, &window);
                         }
                     });

    treeViewModel.ShowActiveProfile();
    window.show();

    return QApplication::exec();
}
