#ifndef FS_ORGANIZER_DOMAIN_MODEL_TREE_NODE_H
#define FS_ORGANIZER_DOMAIN_MODEL_TREE_NODE_H

#include <filesystem>
#include <optional>
#include <vector>

#include "domain/model/Addon.h"

enum class TreeNodeKind : int
{
    Library = 0,
    Category = 1,
    Addon = 2,
};

struct TreeNode
{
    TreeNodeKind kind = TreeNodeKind::Category;
    std::filesystem::path path;
    std::optional<Addon> addon;
    std::vector<TreeNode> children;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_TREE_NODE_H
