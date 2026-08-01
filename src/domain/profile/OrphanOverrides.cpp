#include "domain/profile/OrphanOverrides.h"

#include <algorithm>

#include "domain/support/PathUtils.h"

bool NamesOneOfTheDestinations(const SimulatorProfile& profile, const std::filesystem::path& destination)
{
    return std::ranges::any_of(profile.destinations,
                               [&destination](const std::filesystem::path& known)
                               {
                                   return ComparablePath(known) == ComparablePath(destination);
                               });
}

std::vector<DestinationOverride> OverridesPointingNowhere(const SimulatorProfile& profile)
{
    std::vector<DestinationOverride> orphans;

    for (const DestinationOverride& known : profile.destinationOverrides)
    {
        if (!NamesOneOfTheDestinations(profile, known.destination))
        {
            orphans.push_back(known);
        }
    }

    return orphans;
}

void DropOverridesPointingNowhere(SimulatorProfile& profile)
{
    std::erase_if(profile.destinationOverrides,
                  [&profile](const DestinationOverride& known)
                  {
                      return !NamesOneOfTheDestinations(profile, known.destination);
                  });
}
