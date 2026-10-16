#ifndef FS_ORGANIZER_PLATFORM_PLATFORM_H
#define FS_ORGANIZER_PLATFORM_PLATFORM_H

#include <memory>
#include <vector>

#include "application/model/PlatformCapabilities.h"
#include "application/ports/LegacyConfigSource.h"
#include "application/ports/PathMapper.h"
#include "application/ports/ProcessProbe.h"
#include "application/ports/SimulatorLocator.h"
#include "domain/ports/ArchiveReader.h"
#include "domain/ports/FileOperations.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/LinkService.h"
#include "domain/ports/SidecarStore.h"
#include "infrastructure/platform/AppFolders.h"
#include "infrastructure/platform/InstanceGuard.h"
#include "infrastructure/sim/UserCfgLocation.h"

struct PlatformParts
{
    std::unique_ptr<InstanceGuard> instance{};
    std::unique_ptr<LinkService> links{};
    std::unique_ptr<FilesystemProbe> filesystem{};
    std::unique_ptr<FileOperations> files{};
    std::unique_ptr<SidecarStore> sidecars{};
    std::unique_ptr<ProcessProbe> processes{};
    std::unique_ptr<ArchiveReader> folderArchives{};
    std::unique_ptr<ArchiveReader> packedArchives{};
    std::unique_ptr<SimulatorLocator> locator{};
    std::unique_ptr<LegacyConfigSource> legacyConfig{};
    std::unique_ptr<PathMapper> paths{};
    std::vector<UserCfgLocation> userCfgLocations{};
    AppFolders folders{};
    PlatformCapabilities capabilities{};
};

[[nodiscard]] PlatformParts BuildThePlatform();

#endif // FS_ORGANIZER_PLATFORM_PLATFORM_H
