#ifndef FS_ORGANIZER_DOMAIN_TREE_ADDON_TREE_H
#define FS_ORGANIZER_DOMAIN_TREE_ADDON_TREE_H

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "domain/model/CheckState.h"
#include "domain/model/EnabledAddons.h"
#include "domain/model/TreeNode.h"

[[nodiscard]] std::size_t CountAddons(const TreeNode& node);

[[nodiscard]] std::vector<const TreeNode*> AddonsUnder(const TreeNode& node);

std::vector<const TreeNode*> AddonsUnder(TreeNode&& node) = delete;

[[nodiscard]] const TreeNode* AddonNamed(const std::vector<TreeNode>& libraries, const std::string& nameOrFolder);

const TreeNode* AddonNamed(std::vector<TreeNode>&& libraries, const std::string& nameOrFolder) = delete;

[[nodiscard]] const TreeNode* AddonAt(const std::vector<TreeNode>& libraries, const std::filesystem::path& folder);

const TreeNode* AddonAt(std::vector<TreeNode>&& libraries, const std::filesystem::path& folder) = delete;

[[nodiscard]] const TreeNode* NodeAt(const std::vector<TreeNode>& libraries, const std::filesystem::path& folder);

const TreeNode* NodeAt(std::vector<TreeNode>&& libraries, const std::filesystem::path& folder) = delete;

[[nodiscard]] const TreeNode* AddonHoldingTheIdentity(const std::vector<TreeNode>& libraries,
                                                      const std::filesystem::path& wanted,
                                                      const std::filesystem::path& ignoring);

const TreeNode* AddonHoldingTheIdentity(std::vector<TreeNode>&& libraries,
                                        const std::filesystem::path& wanted,
                                        const std::filesystem::path& ignoring) = delete;

[[nodiscard]] const TreeNode* LibraryTreeAt(const std::vector<TreeNode>& libraries, const std::filesystem::path& root);

const TreeNode* LibraryTreeAt(std::vector<TreeNode>&& libraries, const std::filesystem::path& root) = delete;

[[nodiscard]] std::vector<const TreeNode*> CategoriesUnder(const TreeNode& node);

std::vector<const TreeNode*> CategoriesUnder(TreeNode&& node) = delete;

[[nodiscard]] std::size_t CountCategoriesInside(const TreeNode& node);

[[nodiscard]] bool HoldsAddonsOrWasDeclared(const TreeNode& node);

void AFolderThatGroupsNothingBecomesAnAddon(TreeNode& node);

[[nodiscard]] std::vector<const TreeNode*> CategoriesOfferedIn(const TreeNode& tree, bool offerTheRoot);

std::vector<const TreeNode*> CategoriesOfferedIn(TreeNode&& tree, bool offerTheRoot) = delete;

[[nodiscard]] CheckState DeriveCheckState(const TreeNode& node, const EnabledAddons& enabled);

#endif // FS_ORGANIZER_DOMAIN_TREE_ADDON_TREE_H
