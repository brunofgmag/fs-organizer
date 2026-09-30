#ifndef FS_ORGANIZER_INFRASTRUCTURE_SCENERY_JSON_SCENERY_CACHE_H
#define FS_ORGANIZER_INFRASTRUCTURE_SCENERY_JSON_SCENERY_CACHE_H

#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>

#include "application/ports/SceneryCache.h"

class JsonSceneryCache final : public SceneryCache
{
public:
    explicit JsonSceneryCache(std::filesystem::path filePath);

    [[nodiscard]] std::optional<RememberedScenery> Remember(const std::filesystem::path& addonFolder) const override;

    void Keep(const std::filesystem::path& addonFolder, const RememberedScenery& scenery) override;

    void WriteWhatIsKept() override;

private:
    using Known = std::map<std::string, RememberedScenery, std::less<>>;

    void Read();

    void Write(const Known& snapshot) const;

    std::filesystem::path filePath_;
    mutable std::mutex guard_;
    std::mutex writing_;
    Known known_;
    bool dirty_ = false;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SCENERY_JSON_SCENERY_CACHE_H
