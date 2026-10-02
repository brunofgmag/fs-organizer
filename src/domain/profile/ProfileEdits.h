#ifndef FS_ORGANIZER_DOMAIN_PROFILE_PROFILE_EDITS_H
#define FS_ORGANIZER_DOMAIN_PROFILE_PROFILE_EDITS_H

#include <filesystem>
#include <string>
#include <vector>

#include "domain/model/LibraryId.h"
#include "domain/model/SimulatorProfile.h"

void UnregisterLibrary(SimulatorProfile& profile, const LibraryId& libraryId);

[[nodiscard]] bool RemoveProfile(std::vector<SimulatorProfile>& profiles, const std::string& profileId);

void RepointDestination(SimulatorProfile& profile, const std::filesystem::path& from, const std::filesystem::path& to);

void CarryTheFolder(SimulatorProfile& profile,
                    const Library& library,
                    const std::filesystem::path& from,
                    const std::filesystem::path& to);

void ForgetTheFolder(SimulatorProfile& profile, const Library& library, const std::filesystem::path& folder);

#endif // FS_ORGANIZER_DOMAIN_PROFILE_PROFILE_EDITS_H
