#include <QtWidgets/QApplication>
#include <QtWidgets/QToolButton>

#include "application/ProfileService.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
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
#include "view/MainWindow.h"
#include "view/SetupWizard.h"
#include "viewmodel/CommunityViewModel.h"
#include "viewmodel/SetupViewModel.h"

namespace
{
    constexpr auto kNativeStyle = "windows11";

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
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("FS Organizer"));
    QApplication::setApplicationVersion(QStringLiteral(FSORG_VERSION));
    QApplication::setOrganizationName(QStringLiteral("fs-organizer"));
    QApplication::setStyle(QString::fromLatin1(kNativeStyle));

    WindowsLinkService linkService;
    const WindowsFilesystemProbe filesystemProbe;
    const UuidLibraryIdGenerator identities;
    const JsonManifestParser manifestParser;
    const FilesystemScanner catalog(manifestParser);
    const WindowsSimulatorLocator locator(WindowsUserCfgLocations());
    const WindowsProcessProbe processProbe({"FlightSimulator.exe", "FlightSimulator2024.exe"});
    const SystemClock clock;
    JsonSettingsRepository settings(SettingsFilePath());
    JsonlOperationJournal journal(JournalFilePath());

    if (settings.Load().profiles.empty()
        && !RunSetup(settings, locator, filesystemProbe, identities, catalog))
    {
        return 0;
    }

    const LinkingEngine linking(linkService, filesystemProbe);
    const EntryClassifier classifier(linkService, filesystemProbe);
    ProfileService profileService(catalog, classifier, linking, journal, clock, identities, LinkType::Junction);

    MainWindow window(settings.Load());
    AddonTreeModel model;
    AddonTreeViewModel treeViewModel(profileService, settings, processProbe, model);
    auto* page = new AddonTreePage(treeViewModel, model);

    CommunityModel communityModel;
    CommunityViewModel communityViewModel(profileService, model, communityModel);
    auto* communityPage = new CommunityPage(communityViewModel, communityModel);

    window.AddPage(QObject::tr("Árvore"), page);
    QToolButton* communityButton =
        window.AddPage(QStringLiteral("Community"), communityPage);

    QObject::connect(page, &AddonTreePage::StatusChanged, &window, &MainWindow::ShowStatus);
    QObject::connect(communityPage, &CommunityPage::StatusChanged, &window,
                     &MainWindow::ShowStatus);

    QObject::connect(&treeViewModel, &AddonTreeViewModel::ScanFinished, &communityViewModel,
                     &CommunityViewModel::Show);

    QObject::connect(&communityViewModel, &CommunityViewModel::RepairFinished, page,
                     &AddonTreePage::RefreshUndoState);

    QObject::connect(&window, &MainWindow::PageSelected, &communityViewModel,
                     [&communityViewModel, communityPage](const QWidget* selected)
                     {
                         if (selected == communityPage)
                         {
                             communityViewModel.Show();
                         }
                     });

    QObject::connect(&communityViewModel, &CommunityViewModel::AttentionChanged, communityButton,
                     [communityButton](const std::size_t count)
                     {
                         communityButton->setText(count > 0
                                                      ? QStringLiteral("Community (%1)").arg(count)
                                                      : QStringLiteral("Community"));
                     });
    QObject::connect(&treeViewModel, &AddonTreeViewModel::RestartPendingChanged, &window,
                     &MainWindow::ShowRestartPending);
    QObject::connect(&window, &MainWindow::ProfileChosen, &treeViewModel,
                     &AddonTreeViewModel::ChooseProfile);

    QObject::connect(&window, &MainWindow::AddProfileRequested, &window,
                     [&]
                     {
                         if (RunSetup(settings, locator, filesystemProbe, identities, catalog))
                         {
                             window.ShowProfiles(settings.Load());
                             treeViewModel.ShowActiveProfile();
                         }
                     });

    treeViewModel.ShowActiveProfile();
    window.show();

    return QApplication::exec();
}
