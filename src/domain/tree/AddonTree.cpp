#include "domain/tree/AddonTree.h"

namespace
{
    void CollectAddons(const TreeNode& node, std::vector<const TreeNode*>& addons)
    {
        if (node.kind == TreeNodeKind::Addon)
        {
            addons.push_back(&node);
            return;
        }

        for (const TreeNode& child : node.children)
        {
            CollectAddons(child, addons);
        }
    }
}

std::size_t CountAddons(const TreeNode& node)
{
    if (node.kind == TreeNodeKind::Addon)
    {
        return 1;
    }

    std::size_t addons = 0;
    for (const TreeNode& child : node.children)
    {
        addons += CountAddons(child);
    }

    return addons;
}

std::vector<const TreeNode*> AddonsUnder(const TreeNode& node)
{
    std::vector<const TreeNode*> addons;
    CollectAddons(node, addons);

    return addons;
}

CheckState DeriveCheckState(const TreeNode& node, const EnabledAddons& enabled)
{
    if (node.kind == TreeNodeKind::Addon)
    {
        return enabled.Contains(node.path) ? CheckState::Checked : CheckState::Unchecked;
    }

    std::size_t checked = 0;
    const std::vector<const TreeNode*> addons = AddonsUnder(node);

    for (const TreeNode* addon : addons)
    {
        checked += enabled.Contains(addon->path) ? 1 : 0;
    }

    if (checked == 0)
    {
        return CheckState::Unchecked;
    }

    return checked == addons.size() ? CheckState::Checked : CheckState::Partial;
}
