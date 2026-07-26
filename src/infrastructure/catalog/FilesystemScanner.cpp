#include "infrastructure/catalog/FilesystemScanner.h"

#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include "domain/importing/ImportPaths.h"
#include "domain/model/Manifest.h"

namespace
{
    std::string ReadWholeFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);

        return {std::istreambuf_iterator(file), std::istreambuf_iterator<char>()};
    }

    bool HasManifest(const std::filesystem::path& folder)
    {
        std::error_code error;

        return std::filesystem::exists(ManifestPathIn(folder), error);
    }
}

FilesystemScanner::FilesystemScanner(const ManifestParser& manifestParser, const FilesystemProbe& filesystemProbe)
    : manifestParser_(manifestParser), filesystemProbe_(filesystemProbe)
{
}

TreeNode FilesystemScanner::Scan(const std::filesystem::path& libraryRoot) const
{
    TreeNode root = ScanFolder(libraryRoot);
    root.kind = TreeNodeKind::Library;

    return root;
}

TreeNode FilesystemScanner::ScanFolder(const std::filesystem::path& folder) const
{
    return HasManifest(folder) ? ScanAddon(folder) : ScanCategory(folder);
}

TreeNode FilesystemScanner::ScanAddon(const std::filesystem::path& folder) const
{
    Addon addon;
    addon.folderPath = folder;

    if (const std::optional<Manifest> manifest = manifestParser_.Parse(ReadWholeFile(ManifestPathIn(folder))))
    {
        addon.manifest = *manifest;
    }

    TreeNode node;
    node.kind = TreeNodeKind::Addon;
    node.path = folder;
    node.addon = addon;

    return node;
}

TreeNode FilesystemScanner::ScanCategory(const std::filesystem::path& folder) const
{
    TreeNode node;
    node.kind = TreeNodeKind::Category;
    node.path = folder;

    for (const std::filesystem::path& child : filesystemProbe_.ChildDirectories(folder))
    {
        if (!CreatedByTheImporter(child))
        {
            node.children.push_back(ScanFolder(child));
        }
    }

    return node;
}
