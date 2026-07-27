#include "domain/tree/AddonTree.h"

#include "domain/model/AddonId.h"
#include "domain/support/PathUtils.h"
#include "domain/support/StringUtils.h"

namespace
{
    void CollectCategories(const TreeNode& node, std::vector<const TreeNode*>& categories)
    {
        if (node.kind == TreeNodeKind::Addon)
        {
            return;
        }

        categories.push_back(&node);

        for (const TreeNode& child : node.children)
        {
            CollectCategories(child, categories);
        }
    }

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

std::vector<const TreeNode*> CategoriesUnder(const TreeNode& node)
{
    std::vector<const TreeNode*> categories;
    CollectCategories(node, categories);

    return categories;
}

bool HoldsAddonsOrWasDeclared(const TreeNode& node)
{
    return node.declaredAsCategory || CountAddons(node) > 0;
}

const TreeNode* AddonNamed(const std::vector<TreeNode>& libraries, const std::string& baseName)
{
    for (const TreeNode& library : libraries)
    {
        for (const TreeNode* addon : AddonsUnder(library))
        {
            if (EqualsIgnoringCase(addon->path.filename().string(), baseName))
            {
                return addon;
            }
        }
    }

    return nullptr;
}

const TreeNode* AddonHoldingTheIdentity(const std::vector<TreeNode>& libraries,
                                        const std::filesystem::path& wanted,
                                        const std::filesystem::path& ignoring)
{
    const std::string baseName = wanted.filename().string();
    const std::string excluded = ComparablePath(ignoring);

    for (const TreeNode& library : libraries)
    {
        if (!PathIsInside(wanted, library.path))
        {
            continue;
        }

        for (const TreeNode* addon : AddonsUnder(library))
        {
            if (EqualsIgnoringCase(addon->path.filename().string(), baseName)
                && ComparablePath(addon->path) != excluded)
            {
                return addon;
            }
        }
    }

    return nullptr;
}

const TreeNode* LibraryTreeAt(const std::vector<TreeNode>& libraries, const std::filesystem::path& root)
{
    const std::string wanted = ComparablePath(root);

    for (const TreeNode& library : libraries)
    {
        if (ComparablePath(library.path) == wanted)
        {
            return &library;
        }
    }

    return nullptr;
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
