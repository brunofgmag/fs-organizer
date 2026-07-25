#include <QtWidgets/QApplication>

#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/id/UuidLibraryIdGenerator.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/settings/JsonSettingsRepository.h"
#include "infrastructure/sim/WindowsSimulatorLocator.h"
#include "infrastructure/sim/WindowsUserCfgLocations.h"
#include "view/MainWindow.h"
#include "view/SetupWizard.h"
#include "viewmodel/SetupViewModel.h"

namespace
{
    constexpr auto kNativeStyle = "windows11";

    bool RunSetup(SettingsRepository& settings,
                  const SimulatorLocator& locator,
                  const FileOperations& fileOperations,
                  const LibraryIdGenerator& identities,
                  const CatalogScanner& catalog)
    {
        SetupViewModel viewModel(locator, fileOperations, settings, identities, catalog);
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

    const WindowsFileOperations fileOperations;
    const UuidLibraryIdGenerator identities;
    const JsonManifestParser manifestParser;
    const FilesystemScanner catalog(manifestParser);
    const WindowsSimulatorLocator locator(WindowsUserCfgLocations());
    JsonSettingsRepository settings(SettingsFilePath());

    if (settings.Load().profiles.empty()
        && !RunSetup(settings, locator, fileOperations, identities, catalog))
    {
        return 0;
    }

    MainWindow window(settings.Load());

    QObject::connect(&window, &MainWindow::AddProfileRequested, &window,
                     [&]
                     {
                         if (RunSetup(settings, locator, fileOperations, identities, catalog))
                         {
                             window.ShowProfiles(settings.Load());
                         }
                     });

    window.show();

    return QApplication::exec();
}
