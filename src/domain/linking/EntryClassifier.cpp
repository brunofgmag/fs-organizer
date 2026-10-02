#include "domain/linking/EntryClassifier.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <ranges>
#include <set>

#include "domain/support/PathUtils.h"

namespace
{
    bool IsUnder(const std::string& candidate, const std::string& prefix)
    {
        return candidate.size() > prefix.size() && candidate.compare(0, prefix.size(), prefix) == 0
            && candidate[prefix.size()] == '/';
    }

    [[nodiscard]] std::set<std::string> ComparablePathsOf(const std::vector<std::filesystem::path>& paths)
    {
        std::set<std::string> comparable;

        for (const std::filesystem::path& path : paths)
        {
            comparable.insert(ComparablePath(path));
        }

        return comparable;
    }

    void MarkDuplicates(std::vector<DestinationEntry>& entries)
    {
        std::map<std::string, std::vector<std::size_t>> managedByTarget;

        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            if (CountsAsEnabled(entries[index].classification))
            {
                managedByTarget[ComparablePath(entries[index].target)].push_back(index);
            }
        }

        for (const auto& indexes : managedByTarget | std::views::values)
        {
            if (indexes.size() < 2)
            {
                continue;
            }
            for (const std::size_t index : indexes)
            {
                entries[index].classification = EntryClassification::Duplicated;
            }
        }
    }
}

std::vector<std::filesystem::path> EnabledAddonFolders(const std::vector<DestinationEntry>& entries)
{
    std::vector<std::filesystem::path> folders;
    std::set<std::string> seen;

    for (const DestinationEntry& entry : entries)
    {
        if (!CountsAsEnabled(entry.classification))
        {
            continue;
        }

        if (seen.insert(ComparablePath(entry.target)).second)
        {
            folders.push_back(entry.target);
        }
    }

    return folders;
}

std::vector<std::filesystem::path> LinksPointingAt(const std::vector<DestinationEntry>& entries,
                                                   const std::filesystem::path& addonFolder)
{
    const std::string wanted = ComparablePath(addonFolder);

    std::vector<std::filesystem::path> links;

    for (const DestinationEntry& entry : entries)
    {
        if (CountsAsEnabled(entry.classification) && ComparablePath(entry.target) == wanted)
        {
            links.push_back(entry.path);
        }
    }

    return links;
}

TheAppLinkedPlaces::TheAppLinkedPlaces(const LinkedFolders& linkedFolders) : linkedFolders_(linkedFolders)
{
}

const std::map<std::string, LinkTheAppMade>& TheAppLinkedPlaces::Get() const
{
    if (!places_.has_value())
    {
        std::map<std::string, LinkTheAppMade> made;
        for (const LinkTheAppMade& link : linkedFolders_.WhatTheAppLinked())
        {
            made.emplace(ComparablePath(link.place), link);
        }

        places_ = std::move(made);
    }

    return *places_;
}

ClassificationLookups::ClassificationLookups(const FilesystemProbe& filesystemProbe,
                                             const std::vector<std::filesystem::path>& libraryRoots,
                                             const std::vector<ExternalAddon>& externals)
    : filesystemProbe_(filesystemProbe)
{
    libraryRoots_.reserve(libraryRoots.size());
    for (const std::filesystem::path& root : libraryRoots)
    {
        libraryRoots_.push_back(ComparablePath(root));
    }

    for (const ExternalAddon& external : externals)
    {
        originsByAddonFolder_.emplace(ComparablePath(external.addonFolder), external.externalPath);
        copiesByExternalPath_.emplace(ComparablePath(external.externalPath), external.addonFolder);
    }
}

std::filesystem::path ClassificationLookups::ExternalOrigin(const std::string& comparableAddonFolder) const
{
    const auto known = originsByAddonFolder_.find(comparableAddonFolder);

    return known == originsByAddonFolder_.end() ? std::filesystem::path{} : known->second;
}

std::filesystem::path ClassificationLookups::LibraryCopy(const std::string& comparableExternalPath) const
{
    const auto known = copiesByExternalPath_.find(comparableExternalPath);

    return known == copiesByExternalPath_.end() ? std::filesystem::path{} : known->second;
}

bool ClassificationLookups::IsInsideALibrary(const std::string& comparablePath) const
{
    return std::ranges::any_of(libraryRoots_,
                               [&comparablePath](const std::string& root)
                               {
                                   return IsUnder(comparablePath, root);
                               });
}

bool ClassificationLookups::VolumeIsAvailable(const std::filesystem::path& path) const
{
    const std::filesystem::path root = path.root_path();
    if (root.empty())
    {
        return filesystemProbe_.VolumeIsAvailable(path);
    }

    const auto [known, isNew] = volumes_.try_emplace(ComparablePath(root), false);
    if (isNew)
    {
        known->second = filesystemProbe_.VolumeIsAvailable(path);
    }

    return known->second;
}

std::vector<std::filesystem::path>
EntryClassifier::PlacesUnder(const std::vector<std::filesystem::path>& destinationRoots) const
{
    std::vector<std::filesystem::path> places;

    for (const std::filesystem::path& root : destinationRoots)
    {
        std::ranges::copy(filesystemProbe_.ChildDirectories(root), std::back_inserter(places));
    }

    return places;
}

EntryClassifier::EntryClassifier(const LinkService& linkService,
                                 const FilesystemProbe& filesystemProbe,
                                 const LinkedFolders& linkedFolders)
    : linkService_(linkService), filesystemProbe_(filesystemProbe), linkedFolders_(linkedFolders)
{
}

std::vector<DestinationEntry> EntryClassifier::Resolve(const std::vector<std::filesystem::path>& destinationRoots,
                                                       const std::vector<std::filesystem::path>& libraryRoots,
                                                       const std::vector<ExternalAddon>& externals) const
{
    const std::vector<std::filesystem::path> places = PlacesUnder(destinationRoots);

    const ClassificationLookups lookups(filesystemProbe_, libraryRoots, externals);
    const TheAppLinkedPlaces theAppLinked(linkedFolders_);

    std::vector<DestinationEntry> entries = ClassifyPlaces(places, lookups, theAppLinked);

    MarkDuplicates(entries);

    return entries;
}

std::vector<DestinationEntry> EntryClassifier::ClassifyPlaces(const std::vector<std::filesystem::path>& places,
                                                              const ClassificationLookups& lookups,
                                                              const TheAppLinkedPlaces& theAppLinked) const
{
    std::vector<DestinationEntry> entries;

    if (places.empty())
    {
        return entries;
    }

    const std::vector<std::optional<std::filesystem::path>> targets = linkService_.ReadLinkTargets(places);

    entries.reserve(places.size());

    for (std::size_t index = 0; index < places.size(); ++index)
    {
        entries.push_back(ClassifyEntry(places[index], targets[index], lookups, theAppLinked));
    }

    return entries;
}

std::map<std::string, DestinationEntry>
EntryClassifier::ClassifyByPlace(const std::vector<std::filesystem::path>& places,
                                 const ClassificationLookups& lookups,
                                 const TheAppLinkedPlaces& theAppLinked) const
{
    std::vector<DestinationEntry> entries = ClassifyPlaces(places, lookups, theAppLinked);

    std::map<std::string, DestinationEntry> byPlace;
    for (std::size_t index = 0; index < places.size(); ++index)
    {
        byPlace.emplace(ComparablePath(places[index]), std::move(entries[index]));
    }

    return byPlace;
}

std::vector<DestinationEntry> EntryClassifier::Refresh(const std::vector<DestinationEntry>& known,
                                                       const std::vector<std::filesystem::path>& changed,
                                                       const std::vector<std::filesystem::path>& destinationRoots,
                                                       const std::vector<std::filesystem::path>& libraryRoots,
                                                       const std::vector<ExternalAddon>& externals) const
{
    const std::vector<std::filesystem::path> places = PlacesUnder(destinationRoots);

    const std::set<std::string> touched = ComparablePathsOf(changed);
    const std::set<std::string> present = ComparablePathsOf(places);

    std::map<std::string, const DestinationEntry*> knownByPlace;
    std::set<std::string> retargeted;
    for (const DestinationEntry& entry : known)
    {
        const std::string key = ComparablePath(entry.path);
        knownByPlace.emplace(key, &entry);

        if (!entry.target.empty() && (touched.contains(key) || !present.contains(key)))
        {
            retargeted.insert(ComparablePath(entry.target));
        }
    }

    const ClassificationLookups lookups(filesystemProbe_, libraryRoots, externals);
    const TheAppLinkedPlaces theAppLinked(linkedFolders_);

    std::vector<std::filesystem::path> touchedOrUnknown;
    std::ranges::copy_if(places, std::back_inserter(touchedOrUnknown),
                         [&touched, &knownByPlace](const std::filesystem::path& place)
                         {
                             const std::string key = ComparablePath(place);

                             return touched.contains(key) || !knownByPlace.contains(key);
                         });

    std::map<std::string, DestinationEntry> reclassified = ClassifyByPlace(touchedOrUnknown, lookups, theAppLinked);
    for (const DestinationEntry& entry : reclassified | std::views::values)
    {
        if (!entry.target.empty())
        {
            retargeted.insert(ComparablePath(entry.target));
        }
    }

    std::vector<std::filesystem::path> withAStaleMark;
    std::ranges::copy_if(places, std::back_inserter(withAStaleMark),
                         [&reclassified, &knownByPlace, &retargeted](const std::filesystem::path& place)
                         {
                             const std::string key = ComparablePath(place);
                             if (reclassified.contains(key))
                             {
                                 return false;
                             }

                             const DestinationEntry& before = *knownByPlace.at(key);

                             return !before.target.empty() && retargeted.contains(ComparablePath(before.target));
                         });

    reclassified.merge(ClassifyByPlace(withAStaleMark, lookups, theAppLinked));

    std::vector<DestinationEntry> entries;
    entries.reserve(places.size());

    for (const std::filesystem::path& place : places)
    {
        const std::string key = ComparablePath(place);

        if (const auto fresh = reclassified.find(key); fresh != reclassified.end())
        {
            entries.push_back(std::move(fresh->second));
        }
        else
        {
            entries.push_back(*knownByPlace.at(key));
        }
    }

    MarkDuplicates(entries);

    return entries;
}

std::vector<DestinationEntry> EntryClassifier::LinksAt(const std::vector<std::filesystem::path>& places,
                                                       const std::vector<std::filesystem::path>& libraryRoots,
                                                       const std::vector<ExternalAddon>& externals) const
{
    return LinksAmong(places, TargetsAt(places), libraryRoots, externals);
}

std::vector<std::optional<std::filesystem::path>>
EntryClassifier::TargetsAt(const std::vector<std::filesystem::path>& places) const
{
    return places.empty() ? std::vector<std::optional<std::filesystem::path>>{} : linkService_.ReadLinkTargets(places);
}

std::vector<DestinationEntry>
EntryClassifier::LinksAmong(const std::vector<std::filesystem::path>& places,
                            const std::vector<std::optional<std::filesystem::path>>& targets,
                            const std::vector<std::filesystem::path>& libraryRoots,
                            const std::vector<ExternalAddon>& externals) const
{
    const ClassificationLookups lookups(filesystemProbe_, libraryRoots, externals);

    std::vector<DestinationEntry> links;

    for (std::size_t index = 0; index < places.size(); ++index)
    {
        if (targets[index].has_value())
        {
            links.push_back(ClassifyLink(places[index], *targets[index], lookups));
        }
    }

    return links;
}

DestinationEntry EntryClassifier::WhatStandsWhereALinkWas(const std::filesystem::path& entryPath,
                                                          const ClassificationLookups& lookups,
                                                          const TheAppLinkedPlaces& theAppLinked) const
{
    DestinationEntry entry;
    entry.path = entryPath;
    entry.classification = EntryClassification::Unmanaged;

    const std::map<std::string, LinkTheAppMade>& made = theAppLinked.Get();
    const auto ours = made.find(ComparablePath(entryPath));
    if (ours == made.end() || !APhysicalFolderIsThere(ours->second.libraryCopy, lookups))
    {
        return entry;
    }

    entry.libraryCopy = ours->second.libraryCopy;
    entry.classification = EntryClassification::Substituted;

    return entry;
}

DestinationEntry EntryClassifier::ClassifyEntry(const std::filesystem::path& entryPath,
                                                const std::optional<std::filesystem::path>& target,
                                                const ClassificationLookups& lookups,
                                                const TheAppLinkedPlaces& theAppLinked) const
{
    if (!target.has_value())
    {
        return WhatStandsWhereALinkWas(entryPath, lookups, theAppLinked);
    }

    return ClassifyLink(entryPath, *target, lookups);
}

DestinationEntry EntryClassifier::ClassifyLink(const std::filesystem::path& entryPath,
                                               const std::filesystem::path& target,
                                               const ClassificationLookups& lookups) const
{
    DestinationEntry entry;
    entry.path = entryPath;

    entry.target = NormalizeReparseTarget(target);

    const std::string comparableTarget = ComparablePath(entry.target);
    entry.externalOrigin = lookups.ExternalOrigin(comparableTarget);
    entry.libraryCopy = entry.externalOrigin.empty() ? std::filesystem::path{} : entry.target;

    const std::filesystem::path handedOver = lookups.LibraryCopy(comparableTarget);

    if (!lookups.VolumeIsAvailable(entry.target))
    {
        entry.classification = EntryClassification::Unavailable;
    }
    else if (!filesystemProbe_.TargetDirectoryExists(entry.target))
    {
        entry.classification =
            entry.externalOrigin.empty() ? EntryClassification::Broken : EntryClassification::Vanished;
    }
    else if (BothCopiesAreThere(entry.target, handedOver, lookups))
    {
        entry.externalOrigin = entry.target;
        entry.libraryCopy = handedOver;
        entry.theOtherProgramTookItsFolderBack = true;
        entry.classification = EntryClassification::Divergent;
    }
    else if (!lookups.IsInsideALibrary(comparableTarget))
    {
        entry.classification = EntryClassification::External;
    }
    else
    {
        entry.theOtherProgramTookItsFolderBack = APhysicalFolderIsThere(entry.externalOrigin, lookups);
        entry.classification =
            entry.theOtherProgramTookItsFolderBack ? EntryClassification::Divergent : EntryClassification::Managed;
    }

    return entry;
}

bool EntryClassifier::APhysicalFolderIsThere(const std::filesystem::path& path,
                                             const ClassificationLookups& lookups) const
{
    return !path.empty() && lookups.VolumeIsAvailable(path) && filesystemProbe_.PhysicalDirectoryExists(path);
}

bool EntryClassifier::BothCopiesAreThere(const std::filesystem::path& theOtherPrograms,
                                         const std::filesystem::path& inTheLibrary,
                                         const ClassificationLookups& lookups) const
{
    return !inTheLibrary.empty() && APhysicalFolderIsThere(theOtherPrograms, lookups)
        && APhysicalFolderIsThere(inTheLibrary, lookups);
}
