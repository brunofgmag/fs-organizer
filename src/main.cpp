#include <QtCore/QLibraryInfo>
#include <QtCore/QTranslator>
#include <QtWidgets/QApplication>
#include <QtWidgets/QToolButton>

#include "application/ImportService.h"
#include "application/ProfileService.h"
#include "application/Session.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/id/UuidLibraryIdGenerator.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/settings/JsonSettingsRepository.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "infrastructure/sim/WindowsSimulatorLocator.h"
#include "infrastructure/sim/WindowsUserCfgLocations.h"
#include "view/AddonTreePage.h"
#include "view/CommunityPage.h"
#include "view/JournalPage.h"
#include "view/MainWindow.h"
#include "view/QuarantinePage.h"
#include "view/SetupWizard.h"
#include "view/StagingLeftoverDialog.h"
#include "viewmodel/CommunityViewModel.h"
#include "viewmodel/ImportViewModel.h"
#include "viewmodel/JournalViewModel.h"
#include "viewmodel/QtBackgroundRunner.h"
#include "viewmodel/QuarantineViewModel.h"
#include "viewmodel/SessionNotifier.h"
#include "viewmodel/SetupViewModel.h"

namespace
{
    constexpr auto kNativeStyle = "windows11";
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
        SetupViewModel viewModel(locator, filesystemProbe, settings, identities, catalog);
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
    QApplication::setStyle(QString::fromLatin1(kNativeStyle));

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

    if (settings.Load().profiles.empty() && !RunSetup(settings, locator, filesystemProbe, identities, catalog))
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

    MainWindow window(settings.Load());
    QtBackgroundRunner runner;
    SessionNotifier notifier;
    Session session(profileService, settings, runner, notifier);

    AddonTreeModel model;
    AddonTreeViewModel treeViewModel(session, profileService, processProbe, model, notifier);
    auto* page = new AddonTreePage(treeViewModel, model, notifier);

    ImportViewModel importViewModel(importService, profileService, processProbe, session);

    CommunityModel communityModel;
    CommunityViewModel communityViewModel(profileService, session, notifier, communityModel);
    auto* communityPage = new CommunityPage(communityViewModel, importViewModel, communityModel);

    QuarantineModel quarantineModel;
    QuarantineViewModel quarantineViewModel(importService, profileService, session, notifier, quarantineModel);
    auto* quarantinePage = new QuarantinePage(quarantineViewModel, quarantineModel);

    JournalModel journalModel;
    JournalViewModel journalViewModel(journal, session, journalModel);
    auto* journalPage = new JournalPage(journalViewModel, journalModel);

    window.AddPage(QObject::tr("Árvore"), page);
    QToolButton* communityButton = window.AddPage(QStringLiteral("Community"), communityPage);
    window.AddPage(QObject::tr("Quarentena"), quarantinePage);
    window.AddPage(QObject::tr("Diário"), journalPage);

    QObject::connect(page, &AddonTreePage::StatusChanged, &window, &MainWindow::ShowStatus);
    QObject::connect(communityPage, &CommunityPage::StatusChanged, &window, &MainWindow::ShowStatus);
    QObject::connect(quarantinePage, &QuarantinePage::StatusChanged, &window, &MainWindow::ShowStatus);
    QObject::connect(journalPage, &JournalPage::StatusChanged, &window, &MainWindow::ShowStatus);

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

    QObject::connect(&communityViewModel, &CommunityViewModel::AttentionChanged, communityButton,
                     [communityButton](const std::size_t count)
                     {
                         communityButton->setText(count > 0 ? QStringLiteral("Community (%1)").arg(count)
                                                            : QStringLiteral("Community"));
                     });
    QObject::connect(&treeViewModel, &AddonTreeViewModel::RestartPendingChanged, &window,
                     &MainWindow::ShowRestartPending);
    QObject::connect(&window, &MainWindow::ProfileChosen, &treeViewModel, &AddonTreeViewModel::ChooseProfile);

    QObject::connect(&window, &MainWindow::AddProfileRequested, &window,
                     [&]
                     {
                         if (RunSetup(settings, locator, filesystemProbe, identities, catalog))
                         {
                             window.ShowProfiles(settings.Load());
                             treeViewModel.ShowActiveProfile();
                         }
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
