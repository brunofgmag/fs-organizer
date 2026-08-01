#ifndef FS_ORGANIZER_DOMAIN_PROFILE_ORPHAN_OVERRIDES_H
#define FS_ORGANIZER_DOMAIN_PROFILE_ORPHAN_OVERRIDES_H

#include <filesystem>
#include <vector>

#include "domain/model/SimulatorProfile.h"

[[nodiscard]] bool NamesOneOfTheDestinations(const SimulatorProfile& profile, const std::filesystem::path& destination);

[[nodiscard]] std::vector<DestinationOverride> OverridesPointingNowhere(const SimulatorProfile& profile);

void DropOverridesPointingNowhere(SimulatorProfile& profile);

#endif // FS_ORGANIZER_DOMAIN_PROFILE_ORPHAN_OVERRIDES_H
