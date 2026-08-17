#ifndef FS_ORGANIZER_DOMAIN_TREE_ADDON_DESTINATIONS_H
#define FS_ORGANIZER_DOMAIN_TREE_ADDON_DESTINATIONS_H

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "domain/model/DestinationEntry.h"
#include "domain/model/LibraryId.h"
#include "domain/model/SimulatorProfile.h"

struct AddonDestination
{
    std::filesystem::path destination{};
    std::filesystem::path strayedTo{};
    bool linksNowhere = false;
};

class AddonDestinations
{
public:
    AddonDestinations(const SimulatorProfile& profile, const std::vector<DestinationEntry>& entries);

    [[nodiscard]] AddonDestination Of(const std::filesystem::path& addonFolder) const;

private:
    [[nodiscard]] std::filesystem::path DestinationOf(const std::filesystem::path& addonFolder) const;

    [[nodiscard]] std::filesystem::path Chosen(const LibraryId& libraryId,
                                               const std::filesystem::path& relativePath) const;

    [[nodiscard]] std::filesystem::path StrayedFrom(const std::filesystem::path& addonFolder,
                                                    const std::filesystem::path& destination) const;

    const SimulatorProfile& profile_;
    std::map<std::pair<LibraryId, std::string>, std::filesystem::path> overrides_;
    std::multimap<std::string, std::filesystem::path> linksByTarget_;
    std::set<std::string> brokenLinks_;
};

#endif // FS_ORGANIZER_DOMAIN_TREE_ADDON_DESTINATIONS_H
