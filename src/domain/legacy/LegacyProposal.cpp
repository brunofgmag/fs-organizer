#include "domain/legacy/LegacyProposal.h"

#include <algorithm>
#include <set>
#include <string>

#include "domain/support/PathSegment.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"

namespace
{
    std::vector<std::filesystem::path> WithoutRepeats(const std::vector<std::filesystem::path>& paths)
    {
        std::vector<std::filesystem::path> kept;
        std::set<std::string> seen;

        for (const std::filesystem::path& path : paths)
        {
            if (seen.insert(ComparablePath(path)).second)
            {
                kept.push_back(path);
            }
        }

        return kept;
    }

    std::string WithForwardSlashes(const std::filesystem::path& path)
    {
        std::string text = path.string();
        std::ranges::replace(text, '\\', '/');

        return text;
    }

    bool NamesARoot(const std::filesystem::path& path)
    {
        const std::string key = ComparablePath(path);

        return key.empty() || key.back() == '/' || key.find('/') == std::string::npos;
    }

    std::filesystem::path FolderHolding(const std::filesystem::path& path)
    {
        std::string text = WithForwardSlashes(path);
        const std::size_t lastSeparator = text.find_last_of('/');

        if (lastSeparator == std::string::npos)
        {
            return {};
        }

        text.erase(lastSeparator);

        return {text};
    }

    std::filesystem::path CommonAncestorOf(const std::vector<std::filesystem::path>& paths)
    {
        std::filesystem::path ancestor = paths.front();

        for (const std::filesystem::path& path : paths)
        {
            while (!ancestor.empty() && !PathIsInside(path, ancestor))
            {
                ancestor = FolderHolding(ancestor);
            }

            if (ancestor.empty())
            {
                return {};
            }
        }

        return ancestor;
    }

    std::filesystem::path RelativeToTheRoot(const std::filesystem::path& entry, const std::filesystem::path& root)
    {
        const std::string entryText = WithForwardSlashes(entry);
        const std::size_t rootLength = WithForwardSlashes(root).size();

        if (entryText.size() <= rootLength || entryText[rootLength] != '/')
        {
            return {};
        }

        return {entryText.substr(rootLength + 1)};
    }

    bool EverySegmentIsValid(const std::filesystem::path& relative)
    {
        return std::ranges::all_of(relative,
                                   [](const std::filesystem::path& part)
                                   {
                                       return PathSegment::From(part.string()).has_value();
                                   });
    }

    bool AlreadyRegistered(const SimulatorProfile& current, const std::filesystem::path& root)
    {
        return std::ranges::any_of(current.libraries,
                                   [&root](const Library& library)
                                   {
                                       return ComparablePath(library.path) == ComparablePath(root);
                                   });
    }

    std::set<std::string> CategoriesAlreadyOnDisk(const std::vector<TreeNode>& scanned,
                                                  const std::filesystem::path& root)
    {
        std::set<std::string> present;
        const TreeNode* tree = LibraryTreeAt(scanned, root);

        if (tree == nullptr)
        {
            return present;
        }

        for (const TreeNode* category : CategoriesUnder(*tree))
        {
            if (HoldsAddonsOrWasDeclared(*category))
            {
                present.insert(ComparablePath(category->path));
            }
        }

        return present;
    }

    ProposedLibrary LibraryWithoutCategories(const std::filesystem::path& root, const SimulatorProfile& current)
    {
        ProposedLibrary library;
        library.root = root;
        library.state = AlreadyRegistered(current, root) ? ProposedState::AlreadyPresent : ProposedState::New;

        return library;
    }

    ProposedLibrary LibraryRootedAt(const std::filesystem::path& root,
                                    const std::vector<std::filesystem::path>& entries,
                                    const SimulatorProfile& current,
                                    const std::vector<TreeNode>& scanned)
    {
        ProposedLibrary library = LibraryWithoutCategories(root, current);
        const std::set<std::string> onDisk = library.state == ProposedState::AlreadyPresent
            ? CategoriesAlreadyOnDisk(scanned, root)
            : std::set<std::string>{};

        for (const std::filesystem::path& entry : entries)
        {
            if (ComparablePath(entry) == ComparablePath(root))
            {
                continue;
            }

            const std::filesystem::path relative = RelativeToTheRoot(entry, root);

            if (relative.empty() || !EverySegmentIsValid(relative))
            {
                library.refused.push_back(entry);
                continue;
            }

            const ProposedState state =
                onDisk.contains(ComparablePath(entry)) ? ProposedState::AlreadyPresent : ProposedState::New;

            library.categories.push_back(ProposedCategory{relative, state});
        }

        return library;
    }
}

std::vector<ProposedLibrary> ProposeLibraries(const LegacyInstallation& installation,
                                              const SimulatorProfile& current,
                                              const std::vector<TreeNode>& scanned)
{
    const std::vector<std::filesystem::path> entries = WithoutRepeats(installation.addonPaths);

    if (entries.empty())
    {
        return {};
    }

    const std::filesystem::path ancestor = CommonAncestorOf(entries);

    if (NamesARoot(ancestor))
    {
        std::vector<ProposedLibrary> apart;

        for (const std::filesystem::path& entry : entries)
        {
            apart.push_back(LibraryWithoutCategories(entry, current));
        }

        return apart;
    }

    return {LibraryRootedAt(ancestor, entries, current, scanned)};
}
