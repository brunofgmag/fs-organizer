#ifndef FS_ORGANIZER_DOMAIN_PORTS_LINK_SERVICE_H
#define FS_ORGANIZER_DOMAIN_PORTS_LINK_SERVICE_H

#include <filesystem>
#include <optional>

#include "domain/model/LinkType.h"

class LinkService
{
public:
    virtual ~LinkService() = default;

    virtual bool CreateLink(const std::filesystem::path& linkPath,
                            const std::filesystem::path& target,
                            LinkType linkType) = 0;

    virtual bool RemoveLink(const std::filesystem::path& linkPath) = 0;

    [[nodiscard]] virtual bool IsReparsePoint(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual std::optional<std::filesystem::path>
    ReadLinkTarget(const std::filesystem::path& path) const = 0;
};

#endif
