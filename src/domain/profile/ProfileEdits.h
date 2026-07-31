#ifndef FS_ORGANIZER_DOMAIN_PROFILE_PROFILE_EDITS_H
#define FS_ORGANIZER_DOMAIN_PROFILE_PROFILE_EDITS_H

#include <filesystem>

#include "domain/model/LibraryId.h"
#include "domain/model/SimulatorProfile.h"

void UnregisterLibrary(SimulatorProfile& profile, const LibraryId& libraryId);

void RepointDestination(SimulatorProfile& profile, const std::filesystem::path& from, const std::filesystem::path& to);

#endif // FS_ORGANIZER_DOMAIN_PROFILE_PROFILE_EDITS_H
