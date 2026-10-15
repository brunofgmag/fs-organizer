#include "platform/Platform.h"

#include <string>
#include <utility>

#include "infrastructure/archive/LibarchiveArchiveReader.h"
#include "infrastructure/archive/PosixFolderArchiveReader.h"
#include "infrastructure/fileops/PosixFileOperations.h"
#include "infrastructure/fileops/PosixFilesystemProbe.h"
#include "infrastructure/fileops/PosixSidecarStore.h"
#include "infrastructure/legacy/ProtonLegacyConfigSource.h"
#include "infrastructure/link/PosixLinkService.h"
#include "infrastructure/platform/LinuxInstanceGuard.h"
#include "infrastructure/platform/LinuxKnownFolders.h"
#include "infrastructure/sim/ProcProcessProbe.h"
#include "infrastructure/sim/ProtonSimulatorLocator.h"
#include "infrastructure/sim/ProtonUserCfgLocations.h"

PlatformParts BuildThePlatform()
{
    std::vector<UserCfgLocation> userCfgLocations = ProtonUserCfgLocations();
    std::unique_ptr<SimulatorLocator> locator = std::make_unique<ProtonSimulatorLocator>(userCfgLocations);

    return PlatformParts{
        .instance = std::make_unique<LinuxInstanceGuard>(),
        .links = std::make_unique<PosixLinkService>(),
        .filesystem = std::make_unique<PosixFilesystemProbe>(),
        .files = std::make_unique<PosixFileOperations>(),
        .sidecars = std::make_unique<PosixSidecarStore>(),
        .processes = std::make_unique<ProcProcessProbe>(
            std::vector<std::string>{"FlightSimulator.exe", "FlightSimulator2024.exe"}),
        .folderArchives = std::make_unique<PosixFolderArchiveReader>(),
        .packedArchives = std::make_unique<LibarchiveArchiveReader>(),
        .locator = std::move(locator),
        .legacyConfig = std::make_unique<ProtonLegacyConfigSource>(),
        .userCfgLocations = std::move(userCfgLocations),
        .folders = LinuxAppFolders(),
        .capabilities = {.offersJunctions = false},
    };
}
