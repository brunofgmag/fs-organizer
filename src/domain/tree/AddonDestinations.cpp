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
    : profile_(profile)
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
        if (CountsAsEnabled(entry.classification))
        {
            linksByTarget_.emplace(ComparablePath(entry.target), entry.path);
        }

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

std::filesystem::path AddonDestinations::StrayedFrom(const std::filesystem::path& addonFolder,
                                                     const std::filesystem::path& destination) const
{
    const std::string wanted = ComparablePath(destination);
    const auto [first, last] = linksByTarget_.equal_range(ComparablePath(addonFolder));

    for (auto link = first; link != last; ++link)
    {
        if (ComparablePath(link->second.parent_path()) != wanted)
        {
            return link->second.parent_path();
        }
    }

    return {};
}

AddonDestination AddonDestinations::Of(const std::filesystem::path& addonFolder) const
{
    const std::filesystem::path destination = DestinationOf(addonFolder);

    return {.destination = destination,
            .strayedTo = StrayedFrom(addonFolder, destination),
            .linksNowhere = brokenLinks_.contains(ComparablePath(PathUnder(destination, addonFolder.filename())))};
}
