#ifndef FS_ORGANIZER_DOMAIN_LINKING_LINKING_ENGINE_H
#define FS_ORGANIZER_DOMAIN_LINKING_LINKING_ENGINE_H

#include <filesystem>
#include <optional>

#include "domain/model/Addon.h"
#include "domain/model/LinkOutcome.h"
#include "domain/model/LinkType.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/LinkService.h"

class LinkingEngine
{
public:
    LinkingEngine(LinkService& linkService, const FilesystemProbe& filesystemProbe);

    [[nodiscard]] LinkOutcome
    Enable(const Addon& addon, const std::filesystem::path& destinationRoot, LinkType linkType) const;

    [[nodiscard]] LinkOutcome
    LinkAt(const std::filesystem::path& linkPath, const Addon& addon, LinkType linkType) const;

    [[nodiscard]] LinkOutcome Disable(const std::filesystem::path& linkPath) const;

    [[nodiscard]] std::optional<std::filesystem::path> PointsAt(const std::filesystem::path& linkPath) const;

private:
    LinkService& linkService_;
    const FilesystemProbe& filesystemProbe_;
};

#endif // FS_ORGANIZER_DOMAIN_LINKING_LINKING_ENGINE_H
