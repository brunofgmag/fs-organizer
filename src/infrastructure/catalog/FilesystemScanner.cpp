#include "infrastructure/catalog/FilesystemScanner.h"

#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace
{
    constexpr auto kManifestFileName = "manifest.json";

    std::string ReadWholeFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);

        return {std::istreambuf_iterator(file), std::istreambuf_iterator<char>()};
    }

    std::filesystem::path ManifestPathOf(const std::filesystem::path& folder)
    {
        return folder / kManifestFileName;
    }

    bool HasManifest(const std::filesystem::path& folder)
    {
        std::error_code error;

        return std::filesystem::exists(ManifestPathOf(folder), error);
    }
}

FilesystemScanner::FilesystemScanner(const ManifestParser& manifestParser) : manifestParser_(manifestParser)
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

    if (const std::optional<Manifest> manifest = manifestParser_.Parse(ReadWholeFile(ManifestPathOf(folder))))
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

    std::error_code error;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder, error))
    {
        if (entry.is_directory(error))
        {
            node.children.push_back(ScanFolder(entry.path()));
        }
    }

    return node;
}
