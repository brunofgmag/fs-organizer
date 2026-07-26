#ifndef FS_ORGANIZER_DOMAIN_TREE_CATEGORY_SUGGESTER_H
#define FS_ORGANIZER_DOMAIN_TREE_CATEGORY_SUGGESTER_H

#include <filesystem>
#include <vector>

#include "domain/model/CategoryRule.h"
#include "domain/model/TreeNode.h"

struct CategorySuggestion
{
    std::filesystem::path addonFolder;
    std::filesystem::path currentCategory;
    std::filesystem::path suggestedCategory;
    CategoryRule rule = CategoryRule::None;

    [[nodiscard]] bool Classified() const
    {
        return !suggestedCategory.empty();
    }

    [[nodiscard]] bool WouldMove() const;
};

[[nodiscard]] std::vector<CategorySuggestion> SuggestCategories(const TreeNode& library,
                                                                const std::vector<const TreeNode*>& addons);

#endif // FS_ORGANIZER_DOMAIN_TREE_CATEGORY_SUGGESTER_H
