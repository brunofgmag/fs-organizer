#include <optional>

#include <QtCore/QLibraryInfo>
#include <QtCore/QTranslator>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include "application/ImportService.h"
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
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/preset/FilePresetRepository.h"
#include "infrastructure/settings/JsonSettingsRepository.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "infrastructure/sim/WindowsSimulatorLocator.h"
#include "infrastructure/sim/WindowsUserCfgLocations.h"
#include "support/PathText.h"
#include "view/AddonTreePage.h"
#include "view/CommunityPage.h"
#include "view/JournalPage.h"
#include "view/MainWindow.h"
#include "view/PresetsPage.h"
#include "view/QuarantinePage.h"
#include "view/SetupWizard.h"
#include "view/StagingLeftoverDialog.h"
#include "view/theme/ModernistTheme.h"
#include "view/theme/PageTab.h"
#include "viewmodel/CommunityViewModel.h"
#include "viewmodel/ImportViewModel.h"
#include "viewmodel/JournalViewModel.h"
#include "viewmodel/QtBackgroundRunner.h"
#include "viewmodel/QuarantineViewModel.h"
#include "viewmodel/SessionNotifier.h"
#include "viewmodel/SetupViewModel.h"

namespace
{
    constexpr auto kInterfaceLanguage = "pt_BR";

    void TranslateTheNativeWidgets(QTranslator& translator)
    {
        if (translator.load(QStringLiteral("qtbase_%1").arg(kInterfaceLanguage),
                            QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
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

    if (stored->profiles.empty() && !RunSetup(settings, locator, filesystemProbe, identities, catalog))
    {
        return 0;
    }

    const LinkingEngine linking(linkService, filesystemProbe);
    const EntryClassifier classifier(linkService, filesystemProbe);
    const OperationLog log(journal, clock);
    ProfileService profileService(catalog, classifier, linking, log, identities, LinkType::Junction);

    const ImportEngine importEngine(filesystemProbe, files, linking, log, LinkType::Junction);
    const ImportService importService(importEngine, processProbe, filesystemProbe, catalog, files, linking, log,
                                      LinkType::Junction);

    const LibraryOrganizer organizer(catalog, filesystemProbe, files, linking, classifier, processProbe, log,
                                     LinkType::Junction);

    MainWindow window(settings.Load().value_or(AppSettings{}));
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

    PageTab* libraryButton = window.AddPage(QObject::tr("Biblioteca"), page);
    PageTab* communityButton = window.AddPage(QStringLiteral("Community"), communityPage);
    PageTab* presetsButton = window.AddPage(QObject::tr("Presets"), presetsPage);
    window.AddPage(QObject::tr("Diário"), journalPage);
    PageTab* quarantineButton = window.AddPage(QObject::tr("Quarentena"), quarantinePage);

    window.CarryTriageOn(page);
    window.CarryTriageOn(communityPage);

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

    const auto carryTheSummaryOf = [&window](QWidget* page)
    {
        return [&window, page](const QString& summary)
        {
            window.ShowSummary(page, summary);
        };
    };
    const auto carryTheAsideOf = [&window](QWidget* page)
    {
        return [&window, page](const QString& aside)
        {
            window.ShowAside(page, aside);
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
                         window.ShowTriage(breakdown.broken, breakdown.conflicts, breakdown.unmanaged);
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

    QObject::connect(&notifier, &SessionNotifier::SettingsCouldNotBeSaved, &window,
                     [&]
                     {
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
                         OfferWhatALostImportLeftBehind(importViewModel, &window);
                     });

    treeViewModel.ShowActiveProfile();
    window.show();

    return QApplication::exec();
}
