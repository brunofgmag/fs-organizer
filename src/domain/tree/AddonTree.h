#ifndef FS_ORGANIZER_DOMAIN_TREE_ADDON_TREE_H
#define FS_ORGANIZER_DOMAIN_TREE_ADDON_TREE_H

#include <cstddef>
#include <vector>

#include "domain/model/CheckState.h"
#include "domain/model/EnabledAddons.h"
#include "domain/model/TreeNode.h"

[[nodiscard]] std::size_t CountAddons(const TreeNode& node);

[[nodiscard]] std::vector<const TreeNode*> AddonsUnder(const TreeNode& node);

[[nodiscard]] CheckState DeriveCheckState(const TreeNode& node, const EnabledAddons& enabled);

#endif // FS_ORGANIZER_DOMAIN_TREE_ADDON_TREE_H
