#ifndef FS_ORGANIZER_DOMAIN_LINKING_LINKS_BY_TARGET_H
#define FS_ORGANIZER_DOMAIN_LINKING_LINKS_BY_TARGET_H

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "domain/model/DestinationEntry.h"

class LinksByTarget
{
public:
    LinksByTarget() = default;

    explicit LinksByTarget(const std::vector<DestinationEntry>& entries);

    [[nodiscard]] const std::vector<std::filesystem::path>& PointingAt(const std::filesystem::path& addonFolder) const;

    [[nodiscard]] const std::vector<std::filesystem::path>& PointingAtComparable(const std::string& folderKey) const;

    [[nodiscard]] static std::size_t TimesItWasBuilt();

private:
    std::map<std::string, std::vector<std::filesystem::path>> links_;
};

#endif // FS_ORGANIZER_DOMAIN_LINKING_LINKS_BY_TARGET_H
