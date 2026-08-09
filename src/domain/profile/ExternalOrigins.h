#ifndef FS_ORGANIZER_DOMAIN_PROFILE_EXTERNAL_ORIGINS_H
#define FS_ORGANIZER_DOMAIN_PROFILE_EXTERNAL_ORIGINS_H

#include <filesystem>
#include <vector>

#include "domain/model/ExternalAddon.h"
#include "domain/model/SimulatorProfile.h"

[[nodiscard]] std::vector<ExternalAddon> ExternalAddonsOf(const SimulatorProfile& profile);

[[nodiscard]] std::filesystem::path ExternalOriginOf(const std::vector<ExternalAddon>& externals,
                                                     const std::filesystem::path& addonFolder);

[[nodiscard]] std::filesystem::path LibraryCopyOf(const std::vector<ExternalAddon>& externals,
                                                  const std::filesystem::path& externalPath);

void RememberedByTheLibrary(std::vector<ExternalAddon>& externals,
                            const std::filesystem::path& addonFolder,
                            const std::filesystem::path& externalPath);

void RememberWhereItCameFrom(SimulatorProfile& profile,
                             const std::filesystem::path& addonFolder,
                             const std::filesystem::path& externalPath);

void ForgetWhereItCameFrom(SimulatorProfile& profile, const std::filesystem::path& addonFolder);

#endif // FS_ORGANIZER_DOMAIN_PROFILE_EXTERNAL_ORIGINS_H
