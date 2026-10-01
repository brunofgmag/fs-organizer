#include "domain/profile/ProfileEdits.h"

#include <string>

#include "domain/support/PathUtils.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    std::filesystem::path CarriedTo(const std::filesystem::path& relativePath,
                                    const std::string& moved,
                                    const std::size_t partsMoved,
                                    const std::filesystem::path& landing)
    {
        const std::string key = ComparablePath(relativePath);

        if (key == moved)
        {
            return landing;
        }

        if (key.size() > moved.size() && key.compare(0, moved.size(), moved) == 0 && key[moved.size()] == '/')
        {
            return landing / TailBelow(relativePath, partsMoved);
        }

        return relativePath;
    }
}

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

    std::erase_if(profile.externalOrigins,
                  [&libraryId](const ExternalOrigin& externalOrigin)
                  {
                      return externalOrigin.libraryId == libraryId;
                  });
}

bool RemoveProfile(std::vector<SimulatorProfile>& profiles, const std::string& profileId)
{
    if (profiles.size() <= 1)
    {
        return false;
    }

    return std::erase_if(profiles,
                         [&profileId](const SimulatorProfile& profile)
                         {
                             return profile.id == profileId;
                         })
        > 0;
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

void CarryTheFolder(SimulatorProfile& profile,
                    const Library& library,
                    const std::filesystem::path& from,
                    const std::filesystem::path& to)
{
    const std::filesystem::path leaving = RelativeToLibrary(library, from);
    const std::string moved = ComparablePath(leaving);
    const std::size_t partsMoved = PartsIn(leaving);
    const std::filesystem::path landing = RelativeToLibrary(library, to);

    for (DestinationOverride& known : profile.destinationOverrides)
    {
        if (known.libraryId == library.id)
        {
            known.relativePath = CarriedTo(known.relativePath, moved, partsMoved, landing);
        }
    }

    for (ExternalOrigin& known : profile.externalOrigins)
    {
        if (known.libraryId == library.id)
        {
            known.relativePath = CarriedTo(known.relativePath, moved, partsMoved, landing);
        }
    }
}

void ForgetTheFolder(SimulatorProfile& profile, const Library& library, const std::filesystem::path& folder)
{
    const std::filesystem::path gone = RelativeToLibrary(library, folder);

    std::erase_if(profile.destinationOverrides,
                  [&library, &gone](const DestinationOverride& known)
                  {
                      return known.libraryId == library.id && PathIsInside(known.relativePath, gone);
                  });

    std::erase_if(profile.externalOrigins,
                  [&library, &gone](const ExternalOrigin& known)
                  {
                      return known.libraryId == library.id && PathIsInside(known.relativePath, gone);
                  });
}
