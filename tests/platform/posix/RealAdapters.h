#ifndef FS_ORGANIZER_TESTS_PLATFORM_POSIX_REAL_ADAPTERS_H
#define FS_ORGANIZER_TESTS_PLATFORM_POSIX_REAL_ADAPTERS_H

#include "domain/model/LinkType.h"
#include "infrastructure/fileops/PosixFileOperations.h"
#include "infrastructure/fileops/PosixFilesystemProbe.h"
#include "infrastructure/fileops/PosixSidecarStore.h"
#include "infrastructure/link/PosixLinkService.h"
#include "infrastructure/sim/ProcProcessProbe.h"

using RealFileOperations = PosixFileOperations;
using RealFilesystemProbe = PosixFilesystemProbe;
using RealSidecarStore = PosixSidecarStore;
using RealLinkService = PosixLinkService;
using RealProcessProbe = ProcProcessProbe;

inline constexpr LinkType kRealLinkType = LinkType::Symbolic;

#endif // FS_ORGANIZER_TESTS_PLATFORM_POSIX_REAL_ADAPTERS_H
