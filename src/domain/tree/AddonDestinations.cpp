#include "domain/tree/AddonDestinations.h"

#include "domain/profile/OrphanOverrides.h"
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

AddonDestinations::AddonDestinations(const SimulatorProfile& profile, const std::vector<DestinationEntry>& entries)
    : profile_(profile), defaultKey_(ComparablePath(profile.defaultDestination)), linksByTarget_(entries)
{
    for (const DestinationOverride& candidate : profile.destinationOverrides)
    {
        if (NamesOneOfTheDestinations(profile, candidate.destination))
        {
            overrides_.emplace(std::pair{candidate.libraryId, RelativeKey(candidate.relativePath)},
                               candidate.destination);
        }
    }

    for (const DestinationEntry& entry : entries)
    {
        if (entry.classification == EntryClassification::Broken)
        {
            brokenLinks_.insert(ComparablePath(entry.path));
        }
    }
}

std::filesystem::path AddonDestinations::Chosen(const LibraryId& libraryId,
                                                const std::filesystem::path& relativePath) const
{
    for (std::string key = RelativeKey(relativePath);; key = ParentOf(key))
    {
        const auto match = overrides_.find(std::pair{libraryId, key});

        if (match != overrides_.end())
        {
            return match->second;
        }

        if (key.empty())
        {
            return profile_.defaultDestination;
        }
    }
}

std::filesystem::path AddonDestinations::DestinationOf(const std::filesystem::path& addonFolder) const
{
    const Library* library = LibraryContaining(profile_, addonFolder);
    if (library == nullptr)
    {
        return profile_.defaultDestination;
    }

    return Chosen(library->id, RelativeToLibrary(*library, addonFolder));
}

std::filesystem::path AddonDestinations::StrayedFrom(const std::string& folderKey,
                                                     const std::string& destinationKey) const
{
    for (const std::filesystem::path& link : linksByTarget_.PointingAtComparable(folderKey))
    {
        if (ComparablePath(link.parent_path()) != destinationKey)
        {
            return link.parent_path();
        }
    }

    return {};
}

bool AddonDestinations::IsPinned(const std::filesystem::path& destination, const std::string& destinationKey) const
{
    return !destination.empty() && destinationKey != defaultKey_;
}

AddonDestination AddonDestinations::Of(const std::filesystem::path& addonFolder) const
{
    return Of(addonFolder, ComparablePath(addonFolder));
}

AddonDestination AddonDestinations::Of(const std::filesystem::path& addonFolder, const std::string& folderKey) const
{
    const std::filesystem::path destination = DestinationOf(addonFolder);
    const std::string destinationKey = ComparablePath(destination);
    const bool linksNowhere =
        !brokenLinks_.empty() && brokenLinks_.contains(ComparablePath(PathUnder(destination, addonFolder.filename())));

    return {.destination = destination,
            .strayedTo = StrayedFrom(folderKey, destinationKey),
            .linksNowhere = linksNowhere,
            .linked = !linksByTarget_.PointingAtComparable(folderKey).empty(),
            .pinned = IsPinned(destination, destinationKey)};
}

AddonDestination AddonDestinations::OfAFolderThatIsNotAnAddon(const std::filesystem::path& folder) const
{
    const std::filesystem::path destination = DestinationOf(folder);

    return {.destination = destination,
            .strayedTo = {},
            .linksNowhere = false,
            .linked = false,
            .pinned = IsPinned(destination, ComparablePath(destination))};
}
