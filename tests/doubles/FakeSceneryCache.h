#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_SCENERY_CACHE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_SCENERY_CACHE_H

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>

#include "application/ports/SceneryCache.h"
#include "domain/support/PathUtils.h"

class FakeSceneryCache final : public SceneryCache
{
public:
    [[nodiscard]] std::optional<RememberedScenery> Remember(const std::filesystem::path& addonFolder) const override
    {
        const auto found = known.find(ComparablePath(addonFolder));

        return found == known.end() ? std::nullopt : std::optional(found->second);
    }

    void Keep(const std::filesystem::path& addonFolder, const RememberedScenery& scenery) override
    {
        ++kept;

        known.insert_or_assign(ComparablePath(addonFolder), scenery);
    }

    std::map<std::string, RememberedScenery, std::less<>> known;
    std::size_t kept = 0;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_SCENERY_CACHE_H
