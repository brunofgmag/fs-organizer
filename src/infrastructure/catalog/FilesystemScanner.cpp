#include "infrastructure/catalog/FilesystemScanner.h"

#include <optional>
#include <string>

#include "domain/importing/ImportPaths.h"
#include "domain/model/CategoryMarker.h"
#include "domain/model/Manifest.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"

FilesystemScanner::FilesystemScanner(const ManifestParser& manifestParser,
                                     const FilesystemProbe& filesystemProbe,
                                     const ImportedFolders& importedFolders)
    : manifestParser_(manifestParser), filesystemProbe_(filesystemProbe), importedFolders_(importedFolders)
{
}

TreeNode FilesystemScanner::ScanWhile(const std::filesystem::path& libraryRoot, const ScanGate& gate) const
{
    std::set<std::string> brought;
    for (const std::filesystem::path& folder : importedFolders_.WhatTheImporterBrought())
    {
        brought.insert(ComparablePath(folder));
    }

    TreeNode root = ScanFolder(libraryRoot, brought, gate);
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

TreeNode FilesystemScanner::ScanFolder(const std::filesystem::path& folder,
                                       const std::set<std::string>& brought,
                                       const ScanGate& gate) const
{
    return HasManifest(folder) || brought.contains(ComparablePath(folder)) ? ScanAddon(folder)
                                                                           : ScanCategory(folder, brought, gate);
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

TreeNode FilesystemScanner::ScanCategory(const std::filesystem::path& folder,
                                         const std::set<std::string>& brought,
                                         const ScanGate& gate) const
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
            node.children.push_back(ScanFolder(child, brought, gate));
        }
    }

    return node;
}
