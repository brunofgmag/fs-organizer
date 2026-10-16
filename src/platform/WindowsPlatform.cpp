#include "platform/Platform.h"

#include <string>
#include <utility>

#include "infrastructure/archive/FolderArchiveReader.h"
#include "infrastructure/archive/TarExeArchiveReader.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/fileops/WindowsSidecarStore.h"
#include "infrastructure/legacy/WindowsLegacyConfigSource.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/WindowsInstanceGuard.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/sim/WindowsPathMapper.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "infrastructure/sim/WindowsSimulatorLocator.h"
#include "infrastructure/sim/WindowsUserCfgLocations.h"

namespace
{
    constexpr auto kSingleInstanceName = L"Local\\fs-organizer-single-instance";

    AppFolders WindowsAppFolders()
    {
        return AppFolders{.settingsFile = SettingsFilePath(),
                          .journalFile = JournalFilePath(),
                          .presetsFolder = PresetsFolderPath(),
                          .removedStartupEntriesFolder = RemovedStartupEntriesFolderPath(),
                          .bisectionFolder = BisectionFolderPath(),
                          .sceneryCacheFile = SceneryCacheFilePath(),
                          .documentIndexFile = DocumentIndexFilePath(),
                          .manualFolder = ManualFolderPath()};
    }
}

PlatformParts BuildThePlatform()
{
    std::vector<UserCfgLocation> userCfgLocations = WindowsUserCfgLocations();
    std::unique_ptr<SimulatorLocator> locator = std::make_unique<WindowsSimulatorLocator>(userCfgLocations);

    return PlatformParts{
        .instance = std::make_unique<WindowsInstanceGuard>(kSingleInstanceName),
        .links = std::make_unique<WindowsLinkService>(),
        .filesystem = std::make_unique<WindowsFilesystemProbe>(),
        .files = std::make_unique<WindowsFileOperations>(),
        .sidecars = std::make_unique<WindowsSidecarStore>(),
        .processes = std::make_unique<WindowsProcessProbe>(
            std::vector<std::string>{"FlightSimulator.exe", "FlightSimulator2024.exe"}),
        .folderArchives = std::make_unique<FolderArchiveReader>(),
        .packedArchives = std::make_unique<TarExeArchiveReader>(),
        .locator = std::move(locator),
        .legacyConfig = std::make_unique<WindowsLegacyConfigSource>(ProgramDataFolder()),
        .paths = std::make_unique<WindowsPathMapper>(),
        .userCfgLocations = std::move(userCfgLocations),
        .folders = WindowsAppFolders(),
        .capabilities = {.offersJunctions = true},
    };
}

void IntegrateWithTheDesktop()
{
}
