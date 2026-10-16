#ifndef FS_ORGANIZER_TESTS_PLATFORM_WINDOWS_REAL_ADAPTERS_H
#define FS_ORGANIZER_TESTS_PLATFORM_WINDOWS_REAL_ADAPTERS_H

#include "domain/model/LinkType.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/fileops/WindowsSidecarStore.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/sim/WindowsProcessProbe.h"

using RealFileOperations = WindowsFileOperations;
using RealFilesystemProbe = WindowsFilesystemProbe;
using RealSidecarStore = WindowsSidecarStore;
using RealLinkService = WindowsLinkService;
using RealProcessProbe = WindowsProcessProbe;

inline constexpr LinkType kRealLinkType = LinkType::Junction;

#endif // FS_ORGANIZER_TESTS_PLATFORM_WINDOWS_REAL_ADAPTERS_H
