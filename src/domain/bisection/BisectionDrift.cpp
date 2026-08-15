#include "domain/bisection/BisectionDrift.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>

#include "domain/support/PathUtils.h"

namespace
{
    [[nodiscard]] std::map<std::string, const DestinationEntry*>
    EntriesByPath(const std::vector<DestinationEntry>& entries)
    {
        std::map<std::string, const DestinationEntry*> byPath;

        for (const DestinationEntry& entry : entries)
        {
            byPath.emplace(ComparablePath(entry.path), &entry);
        }

        return byPath;
    }

    [[nodiscard]] std::set<std::string> FoldersOf(const std::vector<std::filesystem::path>& folders)
    {
        std::set<std::string> keys;

        for (const std::filesystem::path& folder : folders)
        {
            keys.insert(ComparablePath(folder));
        }

        return keys;
    }

    [[nodiscard]] bool ItLoaded(const DriftKind kind)
    {
        switch (kind)
        {
        case DriftKind::ALinkWeLeftIsGone:
        case DriftKind::AnEntryWeDidNotLeaveIsThere:
        case DriftKind::AnEntryPointsSomewhereElse:
        case DriftKind::AnAddonLeftTheLibrary: return true;
        case DriftKind::AnAddonJoinedTheLibrary: break;
        }

        return false;
    }

    void CollectTheEntriesThatChanged(const DiskAsItWas& before, const DiskAsItWas& now, std::vector<Divergence>& drift)
    {
        const std::map<std::string, const DestinationEntry*> then = EntriesByPath(before.entries);
        const std::map<std::string, const DestinationEntry*> today = EntriesByPath(now.entries);

        for (const auto& [key, entry] : then)
        {
            const auto still = today.find(key);

            if (still == today.end())
            {
                drift.push_back({.kind = DriftKind::ALinkWeLeftIsGone, .path = entry->path});

                continue;
            }

            if (ComparablePath(still->second->target) != ComparablePath(entry->target))
            {
                drift.push_back({.kind = DriftKind::AnEntryPointsSomewhereElse, .path = entry->path});
            }
        }

        for (const auto& [key, entry] : today)
        {
            if (!then.contains(key))
            {
                drift.push_back({.kind = DriftKind::AnEntryWeDidNotLeaveIsThere, .path = entry->path});
            }
        }
    }

    void CollectTheAddonsThatMoved(const DiskAsItWas& before, const DiskAsItWas& now, std::vector<Divergence>& drift)
    {
        const std::set<std::string> then = FoldersOf(before.libraryAddons);
        const std::set<std::string> today = FoldersOf(now.libraryAddons);

        for (const std::filesystem::path& addon : before.libraryAddons)
        {
            if (!today.contains(ComparablePath(addon)))
            {
                drift.push_back({.kind = DriftKind::AnAddonLeftTheLibrary, .path = addon});
            }
        }

        for (const std::filesystem::path& addon : now.libraryAddons)
        {
            if (!then.contains(ComparablePath(addon)))
            {
                drift.push_back({.kind = DriftKind::AnAddonJoinedTheLibrary, .path = addon});
            }
        }
    }
}

bool NothingThatLoadedMoved(const std::vector<Divergence>& drift)
{
    return std::ranges::all_of(drift,
                               [](const Divergence& divergence)
                               {
                                   return !ItLoaded(divergence.kind);
                               });
}

std::vector<Divergence> DriftBetween(const DiskAsItWas& before, const DiskAsItWas& now)
{
    std::vector<Divergence> drift;

    CollectTheEntriesThatChanged(before, now, drift);
    CollectTheAddonsThatMoved(before, now, drift);

    std::ranges::stable_sort(drift,
                             [](const Divergence& one, const Divergence& other)
                             {
                                 if (one.kind != other.kind)
                                 {
                                     return one.kind < other.kind;
                                 }

                                 return ComparablePath(one.path) < ComparablePath(other.path);
                             });

    return drift;
}
