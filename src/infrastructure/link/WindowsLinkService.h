#ifndef FS_ORGANIZER_INFRASTRUCTURE_LINK_WINDOWS_LINK_SERVICE_H
#define FS_ORGANIZER_INFRASTRUCTURE_LINK_WINDOWS_LINK_SERVICE_H

#include "domain/ports/LinkService.h"

class WindowsLinkService final : public LinkService
{
public:
    [[nodiscard]] bool CreateLink(const std::filesystem::path& linkPath,
                                  const std::filesystem::path& target,
                                  LinkType linkType) override;

    [[nodiscard]] bool RemoveReparseNode(const std::filesystem::path& linkPath) override;

    [[nodiscard]] std::optional<std::filesystem::path> ReadLinkTarget(const std::filesystem::path& path) const override;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_LINK_WINDOWS_LINK_SERVICE_H
