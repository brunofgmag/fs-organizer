#include "infrastructure/catalog/FilesystemScanner.h"

#include <optional>
#include <string>

#include "domain/importing/ImportPaths.h"
#include "domain/model/CategoryMarker.h"
#include "domain/model/Manifest.h"
#include "domain/tree/AddonTree.h"

FilesystemScanner::FilesystemScanner(const ManifestParser& manifestParser, const FilesystemProbe& filesystemProbe)
    : manifestParser_(manifestParser), filesystemProbe_(filesystemProbe)
{
}

TreeNode FilesystemScanner::ScanWhile(const std::filesystem::path& libraryRoot, const ScanGate& gate) const
{
    TreeNode root = ScanFolder(libraryRoot, gate);
    root.kind = TreeNodeKind::Library;

    if (gate.StillWanted())
    {
        AFolderThatGroupsNothingBecomesAnAddon(root);
    }

    return root;
}

bool FilesystemScanner::HasManifest(const std::filesystem::path& folder) const
{
    return filesystemProbe_.EntryExistsWithoutFollowingLinks(ManifestPathIn(folder));
}

bool FilesystemScanner::WasDeclaredACategory(const std::filesystem::path& folder) const
{
    return filesystemProbe_.EntryExistsWithoutFollowingLinks(CategoryMarkerPathIn(folder));
}

TreeNode FilesystemScanner::ScanFolder(const std::filesystem::path& folder, const ScanGate& gate) const
{
    return HasManifest(folder) ? ScanAddon(folder) : ScanCategory(folder, gate);
}

TreeNode FilesystemScanner::ScanAddon(const std::filesystem::path& folder) const
{
    Addon addon;
    addon.folderPath = folder;

    if (const std::optional<std::string> contents = filesystemProbe_.ContentsOf(ManifestPathIn(folder)))
    {
        if (const std::optional<Manifest> manifest = manifestParser_.Parse(*contents))
        {
            addon.manifest = *manifest;
        }
    }

    TreeNode node;
    node.kind = TreeNodeKind::Addon;
    node.path = folder;
    node.addon = addon;

    return node;
}

TreeNode FilesystemScanner::ScanCategory(const std::filesystem::path& folder, const ScanGate& gate) const
{
    TreeNode node;
    node.kind = TreeNodeKind::Category;
    node.path = folder;
    node.declaredAsCategory = WasDeclaredACategory(folder);

    for (const std::filesystem::path& child : filesystemProbe_.ChildDirectories(folder))
    {
        if (!gate.StillWanted())
        {
            break;
        }

        if (!CreatedByTheImporter(child))
        {
            node.children.push_back(ScanFolder(child, gate));
        }
    }

    return node;
}
