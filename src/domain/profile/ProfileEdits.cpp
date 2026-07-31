#include "domain/profile/ProfileEdits.h"

#include <algorithm>
#include <string>

#include "domain/support/PathUtils.h"

void UnregisterLibrary(SimulatorProfile& profile, const LibraryId& libraryId)
{
    std::erase_if(profile.libraries,
                  [&libraryId](const Library& library)
                  {
                      return library.id == libraryId;
                  });

    std::erase_if(profile.destinationOverrides,
                  [&libraryId](const DestinationOverride& destinationOverride)
                  {
                      return destinationOverride.libraryId == libraryId;
                  });
}

void RepointDestination(SimulatorProfile& profile, const std::filesystem::path& from, const std::filesystem::path& to)
{
    const std::string moved = ComparablePath(from);

    const auto namesTheOldPath = [&moved](const std::filesystem::path& candidate)
    {
        return ComparablePath(candidate) == moved;
    };

    for (std::filesystem::path& destination : profile.destinations)
    {
        if (namesTheOldPath(destination))
        {
            destination = to;
        }
    }

    if (namesTheOldPath(profile.defaultDestination))
    {
        profile.defaultDestination = to;
    }

    for (DestinationOverride& destinationOverride : profile.destinationOverrides)
    {
        if (namesTheOldPath(destinationOverride.destination))
        {
            destinationOverride.destination = to;
        }
    }
}
