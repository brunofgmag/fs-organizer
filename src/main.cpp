#include <optional>

#include <QtCore/QDir>
#include <QtCore/QLibraryInfo>
#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include "application/ImportService.h"
#include "application/LegacyConfigImporter.h"
#include "application/LibraryOrganizer.h"
#include "application/PresetService.h"
#include "application/ProfileService.h"
#include "application/Session.h"
#include "application/SetupService.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/id/UuidLibraryIdGenerator.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/legacy/WindowsLegacyConfigSource.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SingleInstance.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/preset/FilePresetRepository.h"
#include "infrastructure/settings/JsonSettingsRepository.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "infrastructure/sim/WindowsSimulatorLocator.h"
#include "infrastructure/sim/WindowsUserCfgLocations.h"
#include "infrastructure/update/GithubUpdateService.h"
#include "support/PathText.h"
#include "view/library/AddonTreePage.h"
#include "view/community/CommunityPage.h"
#include "view/legacy/LegacyImportDialog.h"
#include "view/JournalPage.h"
#include "view/LanguageSwitch.h"
#include "view/options/OptionsPage.h"
#include "view/shell/MainWindow.h"
#include "view/PresetsPage.h"
#include "view/quarantine/QuarantinePage.h"
#include "view/setup/SetupWizard.h"
#include "view/setup/StagingLeftoverDialog.h"
#include "view/theme/ModernistTheme.h"
#include "view/theme/PageTab.h"
#include "viewmodel/CommunityViewModel.h"
#include "viewmodel/ImportViewModel.h"
#include "viewmodel/JournalViewModel.h"
#include "viewmodel/LegacyImportViewModel.h"
#include "viewmodel/OptionsViewModel.h"
#include "viewmodel/QtBackgroundRunner.h"
#include "viewmodel/QuarantineViewModel.h"
#include "viewmodel/SessionNotifier.h"
#include "viewmodel/SetupViewModel.h"
#include "viewmodel/UpdateViewModel.h"

namespace
{
    constexpr auto kSingleInstanceName = L"Local\\fs-organizer-single-instance";
    constexpr auto kDefaultUpdateFeed = "https://api.github.com/repos/brunofgmag/fs-organizer/releases/latest";
    constexpr int kFirstUpdateCheckDelayMs = 3000;

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

    bool RunSetup(SettingsRepository& settings,
                  const SimulatorLocator& locator,
                  const FilesystemProbe& filesystemProbe,
                  const LibraryIdGenerator& identities,
                  const CatalogScanner& catalog)
    {
        SetupService service(locator, filesystemProbe, settings, identities, catalog);
        SetupViewModel viewModel(service);
        viewModel.Detect();

        SetupWizard wizard(viewModel);

        return wizard.exec() == QDialog::Accepted;
    }

    void OfferToDropTheOverridesThatPointNowhere(Session& session, QWidget* parent)
    {
        const std::vector<DestinationOverride> orphans = session.OverridesPointingNowhere();
        if (orphans.empty())
        {
            return;
        }

        QStringList detailed;
        for (const DestinationOverride& orphan : orphans)
        {
            detailed.append(QStringLiteral("%1 -> %2").arg(AsText(orphan.relativePath), AsText(orphan.destination)));
        }

        detailed.append(QString{});
        detailed.append(QObject::tr("Destinations of this profile:"));
        for (const std::filesystem::path& destination : session.Profile().destinations)
        {
            detailed.append(AsText(destination));
        }

        QMessageBox question(
            QMessageBox::Warning, QObject::tr("Destination pinnings pointing outside"),
            QObject::tr(
                "%n destination pinning of this profile names a folder that is not a destination of it. While that is "
                "so, the pinned addons use the default destination. Nothing was deleted from the configuration.",
                nullptr, static_cast<int>(orphans.size())),
            QMessageBox::NoButton, parent);
        question.setDetailedText(detailed.join(QChar::LineFeed));

        const QPushButton* drop = question.addButton(QObject::tr("Discard the pinnings"), QMessageBox::AcceptRole);
        question.addButton(QObject::tr("Keep them and decide later"), QMessageBox::RejectRole);
        question.exec();

        if (question.clickedButton() == drop)
        {
            session.DropOverridesPointingNowhere();
        }
    }

    bool SomethingIsWaitingInTheOldProgram(const std::vector<LegacyMigration>& migrations)
    {
        const auto holdsSomethingNew = [](const MigratableLibrary& library)
        {
            return library.rootExists
                && (library.proposal.state == ProposedState::New
                    || std::ranges::any_of(library.proposal.categories,
                                           [](const ProposedCategory& category)
                                           {
                                               return category.state == ProposedState::New;
                                           }));
        };

        return std::ranges::any_of(migrations,
                                   [&holdsSomethingNew](const LegacyMigration& migration)
                                   {
                                       return std::ranges::any_of(migration.libraries, holdsSomethingNew);
                                   });
    }

    void OfferWhatTheOldProgramKept(LegacyImportViewModel& legacyViewModel, QWidget* parent)
    {
        if (!SomethingIsWaitingInTheOldProgram(legacyViewModel.Migrations()))
        {
            return;
        }

        QMessageBox question(QMessageBox::Question, QObject::tr("MSFS Addons Linker is on this machine"),
                             QObject::tr("It has libraries FS Organizer does not know yet. Nothing is moved or "
                                         "deleted: you choose what to bring over before anything happens."),
                             QMessageBox::NoButton, parent);

        const QPushButton* look =
            question.addButton(QObject::tr("See what can be brought over"), QMessageBox::AcceptRole);
        question.addButton(QObject::tr("Not now"), QMessageBox::RejectRole);
        question.exec();

        if (question.clickedButton() != look)
        {
            return;
        }

        LegacyImportDialog dialog(legacyViewModel, parent);
        dialog.exec();
    }

    void OfferWhatALostImportLeftBehind(ImportViewModel& importViewModel, QWidget* parent)
    {
        const std::vector<StagingLeftover> leftovers = importViewModel.Leftovers();
        if (leftovers.empty())
        {
            return;
        }

        StagingLeftoverDialog dialog(leftovers, parent);
        if (dialog.exec() != QDialog::Accepted)
        {
            return;
        }

        static_cast<void>(importViewModel.DiscardLeftovers(dialog.ToDiscard()));

        if (const std::vector<StagingLeftover> resumed = dialog.ToResume(); !resumed.empty())
        {
            importViewModel.Resume(resumed);
        }
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
    language.Use(LanguageSwitch::Stored({}));

    WindowsLinkService linkService;
    const WindowsFilesystemProbe filesystemProbe;
    WindowsFileOperations files;
    const UuidLibraryIdGenerator identities;
    const JsonManifestParser manifestParser;
    const FilesystemScanner catalog(manifestParser, filesystemProbe);
    const WindowsSimulatorLocator locator(WindowsUserCfgLocations());
    const WindowsProcessProbe processProbe({"FlightSimulator.exe", "FlightSimulator2024.exe"});
    const SystemClock clock;
    JsonSettingsRepository settings(SettingsFilePath());
    JsonlOperationJournal journal(JournalFilePath());

    const std::optional<AppSettings> stored = settings.Load();
    if (!stored.has_value())
    {
        QMessageBox::critical(nullptr, QObject::tr("Unreadable configuration"),
                              QObject::tr("The configuration file exists but could not be read, so FS Organizer will "
                                          "not overwrite it. Move or fix %1 and open the program again.")
                                  .arg(AsText(SettingsFilePath())));

        return 1;
    }

    language.Use(LanguageSwitch::Stored(stored->language));

    const bool setupJustRan = stored->profiles.empty();
    if (setupJustRan && !RunSetup(settings, locator, filesystemProbe, identities, catalog))
    {
        return 0;
    }

    const LinkingEngine linking(linkService, filesystemProbe);
    const EntryClassifier classifier(linkService, filesystemProbe);
    const OperationLog log(journal, clock);

    const AppSettings onDisk = settings.Load().value_or(AppSettings{});
    const LinkType storedLinkType = onDisk.linkType;

    ProfileService profileService(catalog, classifier, linking, log, identities, storedLinkType);

    ImportEngine importEngine(filesystemProbe, files, linking, log, storedLinkType);
    ImportService importService(importEngine, processProbe, filesystemProbe, catalog, files, linking, log,
                                storedLinkType);

    LibraryOrganizer organizer(catalog, filesystemProbe, files, linking, classifier, processProbe, log, storedLinkType);

    MainWindow window(onDisk);
    QtBackgroundRunner runner;
    SessionNotifier notifier;
    Session session(profileService, organizer, settings, processProbe, runner, notifier);

    AddonTreeModel model;
    AddonTreeViewModel treeViewModel(session, profileService, model, notifier);
    auto* page = new AddonTreePage(treeViewModel, model, notifier);

    ImportViewModel importViewModel(importService, profileService, processProbe, session, runner);

    CommunityModel communityModel;
    CommunityViewModel communityViewModel(profileService, session, notifier, communityModel);
    auto* communityPage = new CommunityPage(communityViewModel, importViewModel, communityModel);

    QuarantineModel quarantineModel;
    QuarantineViewModel quarantineViewModel(importService, profileService, session, notifier, quarantineModel);
    auto* quarantinePage = new QuarantinePage(quarantineViewModel, quarantineModel);

    JournalModel journalModel;
    JournalViewModel journalViewModel(journal, session, journalModel);
    auto* journalPage = new JournalPage(journalViewModel, journalModel);

    FilePresetRepository presetRepository(PresetsFolderPath());
    PresetService presetService(presetRepository, profileService);
    PresetViewModel presetViewModel(session, presetService);
    auto* presetsPage = new PresetsPage(presetViewModel, notifier);

    GithubUpdateService updateService(
        qEnvironmentVariable("FSORG_UPDATE_FEED", QString::fromLatin1(kDefaultUpdateFeed)),
        QCoreApplication::applicationVersion(), GithubUpdateService::DefaultUpdatesFolder());
    updateService.DiscardStaged();

    UpdateViewModel updateViewModel(updateService, QCoreApplication::applicationVersion(), onDisk.updateMode,
                                    UpdatesAreOn());

    OptionsViewModel optionsViewModel(session, profileService, settings, notifier);
    auto* optionsPage = new OptionsPage(optionsViewModel, updateViewModel, SettingsFilePath());

    const WindowsLegacyConfigSource legacyConfig(ProgramDataFolder());
    const LegacyConfigImporter legacyImporter(legacyConfig, filesystemProbe);
    LegacyImportViewModel legacyViewModel(session, legacyImporter, presetService);

    PageTab* libraryButton = window.AddPage(QT_TRANSLATE_NOOP("MainWindow", "Library"), page);
    PageTab* communityButton = window.AddPage(QT_TRANSLATE_NOOP("MainWindow", "Destinations"), communityPage);
    PageTab* presetsButton = window.AddPage(QT_TRANSLATE_NOOP("MainWindow", "Presets"), presetsPage);
    window.AddPage(QT_TRANSLATE_NOOP("MainWindow", "Journal"), journalPage);
    PageTab* quarantineButton = window.AddPage(QT_TRANSLATE_NOOP("MainWindow", "Quarantine"), quarantinePage);

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

    QObject::connect(&optionsViewModel, &OptionsViewModel::LanguageChosen, &window,
                     [&language](const QString& chosen)
                     {
                         language.Use(chosen);
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

    QTimer::singleShot(kFirstUpdateCheckDelayMs, &updateViewModel, &UpdateViewModel::CheckQuietly);

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
    QObject::connect(&importViewModel, &ImportViewModel::ConflictResolved, page, adoptWhatChangedOnDisk);
    QObject::connect(&quarantineViewModel, &QuarantineViewModel::Restored, page,
                     [adoptWhatChangedOnDisk](const std::vector<FileOperationResult>&)
                     {
                         adoptWhatChangedOnDisk();
                     });

    QObject::connect(&window, &MainWindow::PageSelected, &window,
                     [&](const QWidget* selected)
                     {
                         if (selected == communityPage)
                         {
                             communityViewModel.Show();
                         }
                         else if (selected == quarantinePage)
                         {
                             quarantineViewModel.Show();
                         }
                         else if (selected == journalPage)
                         {
                             journalViewModel.Show();
                         }
                     });

    QObject::connect(&communityViewModel, &CommunityViewModel::BreakdownChanged, &window,
                     [&window](const AttentionBreakdown& breakdown)
                     {
                         window.ShowTriage(breakdown.broken, breakdown.conflicts, breakdown.duplicated,
                                           breakdown.unmanaged);
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
    QObject::connect(&presetViewModel, &PresetViewModel::Applied, page,
                     [page](const QStringList&)
                     {
                         page->RefreshUndoState();
                     });
    QObject::connect(&window, &MainWindow::ProfileChosen, &treeViewModel, &AddonTreeViewModel::ChooseProfile);

    QObject::connect(&window, &MainWindow::AddProfileRequested, &window,
                     [&]
                     {
                         if (RunSetup(settings, locator, filesystemProbe, identities, catalog))
                         {
                             window.ShowProfiles(settings.Load().value_or(AppSettings{}));
                             treeViewModel.ShowActiveProfile();
                         }
                     });

    QObject::connect(&notifier, &SessionNotifier::ScanFinished, &window,
                     [&window, &settings]
                     {
                         window.ShowProfiles(settings.Load().value_or(AppSettings{}));
                     });

    QObject::connect(&notifier, &SessionNotifier::SettingsCouldNotBeSaved, &window,
                     [&]
                     {
                         window.ShowProfiles(settings.Load().value_or(AppSettings{}));
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
                         OfferWhatALostImportLeftBehind(importViewModel, &window);

                         if (setupJustRan)
                         {
                             OfferWhatTheOldProgramKept(legacyViewModel, &window);
                         }
                     });

    treeViewModel.ShowActiveProfile();
    window.show();

    return QApplication::exec();
}
