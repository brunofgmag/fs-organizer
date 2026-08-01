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
    constexpr auto kInterfaceLanguage = "pt_BR";
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

    void TranslateTheNativeWidgets(QTranslator& translator)
    {
        const QString beside = QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
        const QString name = QStringLiteral("qt_%1").arg(kInterfaceLanguage);

        if (translator.load(name, beside) || translator.load(name, QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        {
            QCoreApplication::installTranslator(&translator);
        }
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
        detailed.append(QObject::tr("Destinos deste perfil:"));
        for (const std::filesystem::path& destination : session.Profile().destinations)
        {
            detailed.append(AsText(destination));
        }

        QMessageBox question(QMessageBox::Warning, QObject::tr("Fixações de destino apontando para fora"),
                             QObject::tr("%1 fixação(ões) de destino deste perfil citam uma pasta que não é destino "
                                         "dele. Enquanto for assim, os addons fixados usam o destino padrão. Nada foi "
                                         "apagado da configuração.")
                                 .arg(orphans.size()),
                             QMessageBox::NoButton, parent);
        question.setDetailedText(detailed.join(QChar::LineFeed));

        const QPushButton* drop = question.addButton(QObject::tr("Descartar as fixações"), QMessageBox::AcceptRole);
        question.addButton(QObject::tr("Manter e decidir depois"), QMessageBox::RejectRole);
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

        QMessageBox question(QMessageBox::Question, QObject::tr("O MSFS Addons Linker está nesta máquina"),
                             QObject::tr("Ele tem bibliotecas que o FS Organizer ainda não conhece. Nada é movido nem "
                                         "apagado: você escolhe o que trazer antes de qualquer coisa acontecer."),
                             QMessageBox::NoButton, parent);

        const QPushButton* look = question.addButton(QObject::tr("Ver o que dá para trazer"), QMessageBox::AcceptRole);
        question.addButton(QObject::tr("Agora não"), QMessageBox::RejectRole);
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

    QTranslator nativeWidgets;
    TranslateTheNativeWidgets(nativeWidgets);

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
        QMessageBox::critical(
            nullptr, QObject::tr("Configuração ilegível"),
            QObject::tr("O arquivo de configuração existe mas não pôde ser lido, então o FS Organizer não vai "
                        "sobrescrevê-lo. Mova ou conserte %1 e abra o programa de novo.")
                .arg(AsText(SettingsFilePath())));

        return 1;
    }

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

    PageTab* libraryButton = window.AddPage(QObject::tr("Biblioteca"), page);
    PageTab* communityButton = window.AddPage(QObject::tr("Destinos"), communityPage);
    PageTab* presetsButton = window.AddPage(QObject::tr("Presets"), presetsPage);
    window.AddPage(QObject::tr("Diário"), journalPage);
    PageTab* quarantineButton = window.AddPage(QObject::tr("Quarentena"), quarantinePage);

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

    QObject::connect(&optionsViewModel, &OptionsViewModel::SettingsCouldNotBeSaved, &window,
                     [&window]
                     {
                         QMessageBox::warning(&window, QObject::tr("Não foi possível salvar"),
                                              QObject::tr("A opção não pôde ser gravada em %1, então ela continua "
                                                          "como estava.")
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
                             &window, QObject::tr("Não foi possível salvar"),
                             QObject::tr("A mudança foi aplicada no disco, mas o perfil não pôde ser gravado em %1. "
                                         "Na próxima abertura ela não vai estar registrada.")
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
