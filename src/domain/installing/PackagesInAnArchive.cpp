#include "domain/installing/PackagesInAnArchive.h"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "domain/importing/ImportPaths.h"
#include "domain/model/Manifest.h"
#include "domain/support/CaseFolding.h"

namespace
{
    using Roots = std::map<std::string, std::vector<PathSegment>>;

    struct Listed
    {
        const ArchiveEntry* entry = nullptr;
        std::vector<PathSegment> segments{};
        std::string lowered{};
    };

    const std::set<std::string>& ReservedDeviceNames()
    {
        static const std::set<std::string> names = []
        {
            std::set<std::string> reserved{"nul", "con", "prn", "aux", "conin$", "conout$"};

            for (const std::string device : {"com", "lpt"})
            {
                for (const char digit : std::string_view{"123456789"})
                {
                    reserved.insert(device + digit);
                }
                for (const std::string superscript : {"\xC2\xB9", "\xC2\xB2", "\xC2\xB3"})
                {
                    reserved.insert(device + superscript);
                }
            }

            return reserved;
        }();

        return names;
    }

    bool IsAReservedDeviceName(const std::string& segment)
    {
        const std::string lowered = LoweredForComparison(segment);
        std::string stem = lowered.substr(0, lowered.find('.'));

        while (stem.ends_with(' '))
        {
            stem.pop_back();
        }

        return ReservedDeviceNames().contains(stem);
    }

    std::optional<std::vector<PathSegment>> SegmentsOf(const std::string& path)
    {
        std::vector<PathSegment> segments;
        std::size_t from = 0;

        while (true)
        {
            const std::size_t slash = path.find('/', from);
            const bool isLast = slash == std::string::npos;
            std::optional<PathSegment> segment = PathSegment::From(path.substr(from, isLast ? slash : slash - from));

            if (!segment || IsAReservedDeviceName(segment->Text()))
            {
                return std::nullopt;
            }

            segments.push_back(std::move(*segment));

            if (isLast)
            {
                return segments;
            }

            from = slash + 1;
        }
    }

    std::set<std::string> FoldersImpliedBy(const std::vector<Listed>& listed)
    {
        std::set<std::string> folders;

        for (const Listed& item : listed)
        {
            for (std::size_t slash = item.lowered.find('/'); slash != std::string::npos;
                 slash = item.lowered.find('/', slash + 1))
            {
                folders.insert(item.lowered.substr(0, slash));
            }
        }

        return folders;
    }

    bool AFileIsAlsoAFolder(const std::vector<Listed>& listed)
    {
        const std::set<std::string> folders = FoldersImpliedBy(listed);

        return std::ranges::any_of(listed,
                                   [&folders](const Listed& item)
                                   {
                                       return item.entry->kind == ArchiveEntryKind::File
                                           && folders.contains(item.lowered);
                                   });
    }

    std::optional<std::vector<Listed>> ReadSafely(const std::vector<ArchiveEntry>& entries)
    {
        std::vector<Listed> listed;
        listed.reserve(entries.size());
        std::set<std::string> seen;

        for (const ArchiveEntry& entry : entries)
        {
            if (entry.kind == ArchiveEntryKind::Other)
            {
                return std::nullopt;
            }

            std::optional<std::vector<PathSegment>> segments = SegmentsOf(entry.path);

            if (!segments)
            {
                return std::nullopt;
            }

            std::string lowered = LoweredForComparison(entry.path);

            if (!seen.insert(lowered).second)
            {
                return std::nullopt;
            }

            listed.push_back(Listed{.entry = &entry, .segments = std::move(*segments), .lowered = std::move(lowered)});
        }

        if (AFileIsAlsoAFolder(listed))
        {
            return std::nullopt;
        }

        return listed;
    }

    std::string NameOf(const std::string& loweredPath)
    {
        return loweredPath.substr(loweredPath.rfind('/') + 1);
    }

    std::string FolderOf(const std::string& loweredPath)
    {
        const std::size_t slash = loweredPath.rfind('/');

        return slash == std::string::npos ? std::string{} : loweredPath.substr(0, slash);
    }

    bool IsAFile(const Listed& item)
    {
        return item.entry->kind == ArchiveEntryKind::File;
    }

    bool IsAManifest(const Listed& item)
    {
        return IsAFile(item) && NameOf(item.lowered) == kManifestFileName;
    }

    Roots FoldersHoldingAManifest(const std::vector<Listed>& listed)
    {
        Roots folders;

        for (const Listed& item : listed)
        {
            if (IsAManifest(item))
            {
                folders.try_emplace(FolderOf(item.lowered), item.segments.begin(), item.segments.end() - 1);
            }
        }

        return folders;
    }

    bool HasAnOuterFolderIn(const Roots& folders, const std::string& folder)
    {
        for (std::size_t slash = folder.find('/'); slash != std::string::npos; slash = folder.find('/', slash + 1))
        {
            if (folders.contains(folder.substr(0, slash)))
            {
                return true;
            }
        }

        return false;
    }

    Roots Outermost(const Roots& folders)
    {
        Roots roots;

        for (const auto& [folder, segments] : folders)
        {
            if (!HasAnOuterFolderIn(folders, folder))
            {
                roots.try_emplace(folder, segments);
            }
        }

        return roots;
    }

    bool TwoRootsShareAName(const Roots& roots)
    {
        std::set<std::string> names;

        for (const auto& [folder, segments] : roots)
        {
            if (!names.insert(NameOf(folder)).second)
            {
                return true;
            }
        }

        return false;
    }

    bool TheAppKeepsTheNameOf(const std::vector<PathSegment>& root)
    {
        const std::filesystem::path name = PathFromUtf8(root.back().Text());

        return CreatedByTheImporter(name) || IsSwapSlot(name);
    }

    bool ARootIsNamedLikeTheAppsOwn(const Roots& roots)
    {
        return std::ranges::any_of(roots,
                                   [](const auto& root)
                                   {
                                       return TheAppKeepsTheNameOf(root.second);
                                   });
    }

    bool HoldsAnInstaller(const std::vector<Listed>& listed)
    {
        return std::ranges::any_of(listed,
                                   [](const Listed& item)
                                   {
                                       const std::string name = NameOf(item.lowered);

                                       return IsAFile(item) && (name.ends_with(".exe") || name.ends_with(".msi"));
                                   });
    }

    Roots::const_iterator RootHolding(const Roots& roots, const Listed& item)
    {
        const std::string& path = item.lowered;

        for (std::size_t slash = path.find('/'); slash != std::string::npos; slash = path.find('/', slash + 1))
        {
            const auto found = roots.find(path.substr(0, slash));

            if (found != roots.end())
            {
                return found;
            }
        }

        return IsAFile(item) ? roots.end() : roots.find(path);
    }

    WhatAnArchiveHolds Refusing(const NothingToInstall refusal)
    {
        return WhatAnArchiveHolds{.refusal = refusal};
    }

    WhatAnArchiveHolds Holding(const std::vector<Listed>& listed, const Roots& roots)
    {
        WhatAnArchiveHolds holds;
        std::map<std::string, std::size_t> packageOf;

        for (const Listed& item : listed)
        {
            const bool isAFile = IsAFile(item);
            const std::uintmax_t bytes = isAFile ? item.entry->bytes : 0;
            const std::size_t files = isAFile ? 1 : 0;
            holds.bytes += bytes;
            holds.files += files;

            const auto root = RootHolding(roots, item);

            if (root == roots.end())
            {
                continue;
            }

            const auto [slot, isNew] = packageOf.try_emplace(root->first, holds.packages.size());

            if (isNew)
            {
                holds.packages.push_back(PackageInAnArchive{.root = root->second});
            }

            PackageInAnArchive& package = holds.packages[slot->second];
            package.bytes += bytes;
            package.files += files;

            if (IsAManifest(item) && FolderOf(item.lowered) == root->first)
            {
                package.manifest = item.entry->path;
            }
        }

        return holds;
    }
}

WhatAnArchiveHolds PackagesIn(const std::vector<ArchiveEntry>& entries)
{
    const std::optional<std::vector<Listed>> listed = ReadSafely(entries);

    if (!listed)
    {
        return Refusing(NothingToInstall::AnEntryIsNotSafe);
    }

    const Roots folders = FoldersHoldingAManifest(*listed);

    if (folders.contains(std::string{}))
    {
        return Refusing(NothingToInstall::ThePackageHasNoFolder);
    }

    const Roots roots = Outermost(folders);

    if (TwoRootsShareAName(roots))
    {
        return Refusing(NothingToInstall::TwoPackagesShareAName);
    }

    if (ARootIsNamedLikeTheAppsOwn(roots))
    {
        return Refusing(NothingToInstall::ThePackageNameIsReserved);
    }

    if (roots.empty())
    {
        return Refusing(HoldsAnInstaller(*listed) ? NothingToInstall::OnlyAnInstaller : NothingToInstall::NoPackage);
    }

    return Holding(*listed, roots);
}

const PathSegment& FolderNameOf(const PackageInAnArchive& package)
{
    return package.root.back();
}

std::string EntryPathOf(const PackageInAnArchive& package)
{
    std::string path;

    for (const PathSegment& segment : package.root)
    {
        path += path.empty() ? segment.Text() : "/" + segment.Text();
    }

    return path;
}
