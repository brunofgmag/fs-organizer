#include "domain/linking/EntryClassifier.h"

#include <algorithm>
#include <map>
#include <ranges>
#include <set>

#include "domain/profile/ExternalOrigins.h"
#include "domain/support/PathUtils.h"

namespace
{
    bool IsUnder(const std::filesystem::path& path, const std::filesystem::path& root)
    {
        const std::string candidate = ComparablePath(path);
        const std::string prefix = ComparablePath(root);
        return candidate.size() > prefix.size() && candidate.compare(0, prefix.size(), prefix) == 0
            && candidate[prefix.size()] == '/';
    }

    bool IsUnderAny(const std::filesystem::path& path, const std::vector<std::filesystem::path>& roots)
    {
        return std::ranges::any_of(roots,
                                   [&path](const std::filesystem::path& root)
                                   {
                                       return IsUnder(path, root);
                                   });
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

EntryClassifier::EntryClassifier(const LinkService& linkService, const FilesystemProbe& filesystemProbe)
    : linkService_(linkService), filesystemProbe_(filesystemProbe)
{
}

std::vector<DestinationEntry> EntryClassifier::Resolve(const std::vector<std::filesystem::path>& destinationRoots,
                                                       const std::vector<std::filesystem::path>& libraryRoots,
                                                       const std::vector<ExternalAddon>& externals) const
{
    std::vector<DestinationEntry> entries;

    for (const std::filesystem::path& root : destinationRoots)
    {
        for (const std::filesystem::path& child : filesystemProbe_.ChildDirectories(root))
        {
            entries.push_back(ClassifyEntry(child, libraryRoots, externals));
        }
    }

    MarkDuplicates(entries);

    return entries;
}

DestinationEntry EntryClassifier::ClassifyEntry(const std::filesystem::path& entryPath,
                                                const std::vector<std::filesystem::path>& libraryRoots,
                                                const std::vector<ExternalAddon>& externals) const
{
    DestinationEntry entry;
    entry.path = entryPath;

    const std::optional<std::filesystem::path> target = linkService_.ReadLinkTarget(entryPath);
    if (!target.has_value())
    {
        entry.classification = EntryClassification::Unmanaged;
        return entry;
    }

    entry.target = NormalizeReparseTarget(*target);
    entry.externalOrigin = ExternalOriginOf(externals, entry.target);

    if (!filesystemProbe_.VolumeIsAvailable(entry.target))
    {
        entry.classification = EntryClassification::Unavailable;
    }
    else if (!filesystemProbe_.TargetDirectoryExists(entry.target))
    {
        entry.classification =
            entry.externalOrigin.empty() ? EntryClassification::Broken : EntryClassification::Vanished;
    }
    else if (!IsUnderAny(entry.target, libraryRoots))
    {
        entry.classification = EntryClassification::External;
    }
    else
    {
        entry.theOtherProgramTookItsFolderBack = TheOtherProgramTookItsFolderBack(entry.externalOrigin);
        entry.classification =
            entry.theOtherProgramTookItsFolderBack ? EntryClassification::Divergent : EntryClassification::Managed;
    }

    return entry;
}

bool EntryClassifier::TheOtherProgramTookItsFolderBack(const std::filesystem::path& externalPath) const
{
    return !externalPath.empty() && filesystemProbe_.VolumeIsAvailable(externalPath)
        && filesystemProbe_.PhysicalDirectoryExists(externalPath);
}
