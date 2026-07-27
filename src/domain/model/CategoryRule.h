#ifndef FS_ORGANIZER_DOMAIN_MODEL_CATEGORY_RULE_H
#define FS_ORGANIZER_DOMAIN_MODEL_CATEGORY_RULE_H

#include <array>
#include <cstddef>

enum class CategoryRule : int
{
    None = 0,
    TheNameSaysAirport = 1,
    TheNameSaysTraffic = 2,
    TheContentTypeIsScenery = 3,
    TheContentTypeIsSound = 4,
    TheContentTypeIsLivery = 5,
};

inline constexpr std::array kAllCategoryRules{
    CategoryRule::None,
    CategoryRule::TheNameSaysAirport,
    CategoryRule::TheNameSaysTraffic,
    CategoryRule::TheContentTypeIsScenery,
    CategoryRule::TheContentTypeIsSound,
    CategoryRule::TheContentTypeIsLivery,
};

static_assert(kAllCategoryRules.size() == static_cast<std::size_t>(CategoryRule::TheContentTypeIsLivery) + 1,
              "Every CategoryRule belongs in kAllCategoryRules, and the last one carries the highest value.");

[[nodiscard]] constexpr bool TrustedOnItsOwn(const CategoryRule rule)
{
    switch (rule)
    {
    case CategoryRule::TheNameSaysAirport:
    case CategoryRule::TheNameSaysTraffic:
    case CategoryRule::TheContentTypeIsScenery:
    case CategoryRule::TheContentTypeIsSound: return true;
    case CategoryRule::TheContentTypeIsLivery:
    case CategoryRule::None: return false;
    }

    return false;
}

#endif // FS_ORGANIZER_DOMAIN_MODEL_CATEGORY_RULE_H
