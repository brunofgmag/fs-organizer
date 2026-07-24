#include "domain/linking/EnabledStateResolver.h"

#include <algorithm>
#include <map>
#include <ranges>
#include <set>

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
        return std::ranges::any_of(roots, [&path](const std::filesystem::path& root)
        {
            return IsUnder(path, root);
        });
    }

    void MarkDuplicates(std::vector<DestinationEntry>& entries)
    {
        std::map<std::string, std::vector<std::size_t>> managedByTarget;

        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            if (entries[index].classification == EntryClassification::Managed)
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
        if (entry.classification != EntryClassification::Managed
            && entry.classification != EntryClassification::Duplicated)
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

EnabledStateResolver::EnabledStateResolver(const LinkService& linkService, const FileOperations& fileOperations)
    : linkService_(linkService), fileOperations_(fileOperations)
{
}

std::vector<DestinationEntry> EnabledStateResolver::Resolve(
    const std::vector<std::filesystem::path>& destinationRoots,
    const std::vector<std::filesystem::path>& libraryRoots) const
{
    std::vector<DestinationEntry> entries;

    for (const std::filesystem::path& root : destinationRoots)
    {
        for (const std::filesystem::path& child : fileOperations_.ChildDirectories(root))
        {
            entries.push_back(ClassifyEntry(child, libraryRoots));
        }
    }

    MarkDuplicates(entries);

    return entries;
}

DestinationEntry EnabledStateResolver::ClassifyEntry(
    const std::filesystem::path& entryPath,
    const std::vector<std::filesystem::path>& libraryRoots) const
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

    if (!fileOperations_.VolumeIsAvailable(entry.target))
    {
        entry.classification = EntryClassification::Unavailable;
    }
    else if (!fileOperations_.TargetDirectoryExists(entry.target))
    {
        entry.classification = EntryClassification::Broken;
    }
    else
    {
        entry.classification = IsUnderAny(entry.target, libraryRoots)
                                   ? EntryClassification::Managed
                                   : EntryClassification::External;
    }

    return entry;
}
