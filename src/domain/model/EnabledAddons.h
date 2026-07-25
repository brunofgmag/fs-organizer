#ifndef FS_ORGANIZER_DOMAIN_MODEL_ENABLED_ADDONS_H
#define FS_ORGANIZER_DOMAIN_MODEL_ENABLED_ADDONS_H

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "domain/support/PathUtils.h"

class EnabledAddons
{
public:
    EnabledAddons() = default;

    explicit EnabledAddons(const std::vector<std::filesystem::path>& folders)
    {
        for (const std::filesystem::path& folder : folders)
        {
            folders_.insert(ComparablePath(folder));
        }
    }

    [[nodiscard]] bool Contains(const std::filesystem::path& folder) const
    {
        return folders_.contains(ComparablePath(folder));
    }

private:
    std::set<std::string> folders_;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_ENABLED_ADDONS_H
