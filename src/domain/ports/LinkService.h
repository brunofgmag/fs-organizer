#ifndef FS_ORGANIZER_DOMAIN_PORTS_LINK_SERVICE_H
#define FS_ORGANIZER_DOMAIN_PORTS_LINK_SERVICE_H

#include <filesystem>
#include <optional>
#include <vector>

#include "domain/model/LinkFailure.h"
#include "domain/model/LinkType.h"

class LinkService
{
public:
    virtual ~LinkService() = default;

    [[nodiscard]] virtual LinkFailure
    CreateLink(const std::filesystem::path& linkPath, const std::filesystem::path& target, LinkType linkType) = 0;

    [[nodiscard]] virtual bool RemoveReparseNode(const std::filesystem::path& linkPath) = 0;

    [[nodiscard]] virtual std::optional<std::filesystem::path>
    ReadLinkTarget(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual std::vector<std::optional<std::filesystem::path>>
    ReadLinkTargets(const std::vector<std::filesystem::path>& paths) const
    {
        std::vector<std::optional<std::filesystem::path>> targets;
        targets.reserve(paths.size());

        for (const std::filesystem::path& path : paths)
        {
            targets.push_back(ReadLinkTarget(path));
        }

        return targets;
    }
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_LINK_SERVICE_H
