#include "platform/Platform.h"

#include <string>
#include <utility>

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include "infrastructure/archive/LibarchiveArchiveReader.h"
#include "infrastructure/archive/PosixFolderArchiveReader.h"
#include "infrastructure/fileops/PosixFileOperations.h"
#include "infrastructure/fileops/PosixFilesystemProbe.h"
#include "infrastructure/fileops/PosixSidecarStore.h"
#include "infrastructure/legacy/ProtonLegacyConfigSource.h"
#include "infrastructure/link/PosixLinkService.h"
#include "infrastructure/platform/AppImageDesktopEntry.h"
#include "infrastructure/platform/DesktopIconReload.h"
#include "infrastructure/platform/LinuxInstanceGuard.h"
#include "infrastructure/platform/LinuxKnownFolders.h"
#include "infrastructure/sim/ProcProcessProbe.h"
#include "infrastructure/sim/ProtonPathMapper.h"
#include "infrastructure/sim/ProtonPrefixes.h"
#include "infrastructure/sim/ProtonSimulatorLocator.h"
#include "infrastructure/sim/ProtonUserCfgLocations.h"
#include "support/PathText.h"

PlatformParts BuildThePlatform()
{
    const std::vector<ProtonPrefix> prefixes = ProtonPrefixes(SteamLibraries(SteamRoots()));
    std::vector<UserCfgLocation> userCfgLocations = ProtonUserCfgLocations(prefixes);
    std::unique_ptr<PathMapper> paths = std::make_unique<ProtonPathMapper>(prefixes);
    std::unique_ptr<SimulatorLocator> locator = std::make_unique<ProtonSimulatorLocator>(userCfgLocations, *paths);

    return PlatformParts{
        .instance = std::make_unique<LinuxInstanceGuard>(RuntimeFolder() / "fs-organizer.lock"),
        .links = std::make_unique<PosixLinkService>(),
        .filesystem = std::make_unique<PosixFilesystemProbe>(),
        .files = std::make_unique<PosixFileOperations>(),
        .sidecars = std::make_unique<PosixSidecarStore>(),
        .processes = std::make_unique<ProcProcessProbe>(
            std::vector<std::string>{"FlightSimulator.exe", "FlightSimulator2024.exe"}),
        .folderArchives = std::make_unique<PosixFolderArchiveReader>(),
        .packedArchives = std::make_unique<LibarchiveArchiveReader>(),
        .locator = std::move(locator),
        .legacyConfig = std::make_unique<ProtonLegacyConfigSource>(prefixes),
        .paths = std::move(paths),
        .userCfgLocations = std::move(userCfgLocations),
        .folders = LinuxAppFolders(),
        .capabilities = {.offersJunctions = false},
    };
}

void IntegrateWithTheDesktop()
{
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (appImage.isEmpty() || !QFileInfo(appImage).isFile())
    {
        return;
    }

    QFile shippedEntry(QStringLiteral(":/linux/fs-organizer.desktop"));
    QFile icon(QStringLiteral(":/icons/app-icon_256.png"));
    if (!shippedEntry.open(QIODevice::ReadOnly) || !icon.open(QIODevice::ReadOnly))
    {
        return;
    }

    const DesktopEntryInstall installed = InstallTheDesktopEntry(
        AsPath(appImage), DataHomeFolder(), shippedEntry.readAll().toStdString(), icon.readAll().toStdString());

    if (installed.iconWritten)
    {
        RefreshTheIconThemeCacheIn(IconThemeUnder(DataHomeFolder()));
        AskTheDesktopToReloadItsIcons();
    }
}
