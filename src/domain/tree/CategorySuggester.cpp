#include "domain/tree/CategorySuggester.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"

namespace
{
    struct BuiltInRule
    {
        CategoryRule rule = CategoryRule::None;
        std::string_view inTheName;
        std::string_view contentType;
        std::string_view categoryName;
    };

    inline constexpr std::array kBuiltInRules{
        BuiltInRule{.rule = CategoryRule::TheNameSaysTraffic,
                    .inTheName = "traffic",
                    .contentType = {},
                    .categoryName = "traffic"},
        BuiltInRule{.rule = CategoryRule::TheNameSaysAirport,
                    .inTheName = "airport",
                    .contentType = {},
                    .categoryName = "sceneries"},
        BuiltInRule{.rule = CategoryRule::TheContentTypeIsScenery,
                    .inTheName = {},
                    .contentType = "scenery",
                    .categoryName = "sceneries"},
        BuiltInRule{.rule = CategoryRule::TheContentTypeIsSound,
                    .inTheName = {},
                    .contentType = "sound",
                    .categoryName = "sounds"},
        BuiltInRule{.rule = CategoryRule::TheContentTypeIsLivery,
                    .inTheName = {},
                    .contentType = "livery",
                    .categoryName = "liveries"},
    };

    std::string Folded(const std::string& text)
    {
        std::string folded = text;
        std::ranges::transform(folded, folded.begin(),
                               [](const unsigned char character)
                               {
                                   return static_cast<char>(std::tolower(character));
                               });

        return folded;
    }

    bool Matches(const BuiltInRule& rule, const std::string& baseName, const std::string& contentType)
    {
        if (!rule.inTheName.empty())
        {
            return baseName.find(rule.inTheName) != std::string::npos;
        }

        return contentType.compare(0, rule.contentType.size(), rule.contentType) == 0;
    }

    const TreeNode* CategoryNamed(const TreeNode& library, const std::string_view name)
    {
        for (const TreeNode* category : CategoriesUnder(library))
        {
            if (Folded(category->path.filename().string()) == name)
            {
                return category;
            }
        }

        return nullptr;
    }

    CategorySuggestion SuggestOne(const TreeNode& library, const TreeNode& addon)
    {
        CategorySuggestion suggestion{.addonFolder = addon.path,
                                      .currentCategory = addon.path.parent_path(),
                                      .suggestedCategory = {},
                                      .rule = CategoryRule::None};

        const std::string baseName = Folded(addon.path.filename().string());
        const std::string contentType =
            addon.addon.has_value() ? Folded(addon.addon->manifest.contentType) : std::string{};

        for (const BuiltInRule& rule : kBuiltInRules)
        {
            if (!Matches(rule, baseName, contentType))
            {
                continue;
            }

            const TreeNode* category = CategoryNamed(library, rule.categoryName);
            if (category == nullptr)
            {
                return suggestion;
            }

            suggestion.suggestedCategory = category->path;
            suggestion.rule = rule.rule;

            return suggestion;
        }

        return suggestion;
    }
}

bool CategorySuggestion::WouldMove() const
{
    return Classified() && ComparablePath(suggestedCategory) != ComparablePath(currentCategory);
}

std::vector<CategorySuggestion> SuggestCategories(const TreeNode& library, const std::vector<const TreeNode*>& addons)
{
    std::vector<CategorySuggestion> suggestions;
    suggestions.reserve(addons.size());

    for (const TreeNode* addon : addons)
    {
        suggestions.push_back(SuggestOne(library, *addon));
    }

    return suggestions;
}
