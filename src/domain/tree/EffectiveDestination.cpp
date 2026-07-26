#include "domain/tree/EffectiveDestination.h"

#include <algorithm>
#include <ranges>
#include <string>

#include "domain/support/PathUtils.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    std::string RelativeKey(const std::filesystem::path& relativePath)
    {
        const std::string key = ComparablePath(relativePath);

        return key == "." ? std::string{} : key;
    }

    std::string ParentOf(const std::string& key)
    {
        const std::size_t separator = key.rfind('/');

        return separator == std::string::npos ? std::string{} : key.substr(0, separator);
    }
}

std::filesystem::path EffectiveDestination(const SimulatorProfile& profile,
                                           const LibraryId& libraryId,
                                           const std::filesystem::path& relativePath)
{
    for (std::string key = RelativeKey(relativePath);; key = ParentOf(key))
    {
        const auto match = std::ranges::find_if(profile.destinationOverrides,
                                                [&libraryId, &key](const DestinationOverride& candidate)
                                                {
                                                    return candidate.libraryId == libraryId
                                                        && RelativeKey(candidate.relativePath) == key;
                                                });

        if (match != profile.destinationOverrides.end())
        {
            return match->destination;
        }

        if (key.empty())
        {
            return profile.defaultDestination;
        }
    }
}

std::filesystem::path EffectiveDestination(const SimulatorProfile& profile, const std::filesystem::path& addonFolder)
{
    const Library* library = LibraryContaining(profile, addonFolder);
    if (library == nullptr)
    {
        return profile.defaultDestination;
    }

    return EffectiveDestination(profile, library->id, RelativeToLibrary(*library, addonFolder));
}

std::filesystem::path PlannedLinkPath(const SimulatorProfile& profile, const std::filesystem::path& addonFolder)
{
    return EffectiveDestination(profile, addonFolder) / addonFolder.filename();
}
