#ifndef FS_ORGANIZER_DOMAIN_MODEL_TREE_NODE_H
#define FS_ORGANIZER_DOMAIN_MODEL_TREE_NODE_H

#include <filesystem>
#include <optional>
#include <vector>

#include "domain/model/Addon.h"
#include "domain/model/CheckState.h"
#include "domain/model/TreeNodeKind.h"

struct TreeNode
{
    TreeNodeKind kind = TreeNodeKind::Category;
    std::filesystem::path path;
    CheckState checkState = CheckState::Unchecked;
    std::optional<std::filesystem::path> destinationOverride;
    std::optional<Addon> addon;
    std::vector<TreeNode> children;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_TREE_NODE_H
