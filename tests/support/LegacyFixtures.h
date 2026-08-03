#ifndef FS_ORGANIZER_TESTS_SUPPORT_LEGACY_FIXTURES_H
#define FS_ORGANIZER_TESTS_SUPPORT_LEGACY_FIXTURES_H

#include <filesystem>
#include <utility>
#include <vector>

#include "domain/model/Library.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/model/TreeNode.h"

[[nodiscard]] inline SimulatorProfile ProfileHolding(const std::vector<std::filesystem::path>& libraryPaths)
{
    SimulatorProfile profile;
    profile.id = "msfs2024";

    for (const std::filesystem::path& path : libraryPaths)
    {
        profile.libraries.push_back(Library{.id = "library-1", .path = path, .label = "MSFS 2024"});
    }

    return profile;
}

[[nodiscard]] inline TreeNode CategoryAt(const std::filesystem::path& path)
{
    TreeNode node;
    node.kind = TreeNodeKind::Category;
    node.path = path;

    return node;
}

[[nodiscard]] inline TreeNode CategoryHolding(const std::filesystem::path& path)
{
    TreeNode addon;
    addon.kind = TreeNodeKind::Addon;
    addon.path = path / "an-addon";

    TreeNode category = CategoryAt(path);
    category.children.push_back(std::move(addon));

    return category;
}

[[nodiscard]] inline std::vector<TreeNode> LibraryScannedAt(const std::filesystem::path& root,
                                                            const std::vector<std::filesystem::path>& categories)
{
    TreeNode library;
    library.kind = TreeNodeKind::Library;
    library.path = root;

    for (const std::filesystem::path& category : categories)
    {
        library.children.push_back(CategoryHolding(category));
    }

    return {library};
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_LEGACY_FIXTURES_H
