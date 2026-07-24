#ifndef FS_ORGANIZER_DOMAIN_LINKING_ENABLED_STATE_RESOLVER_H
#define FS_ORGANIZER_DOMAIN_LINKING_ENABLED_STATE_RESOLVER_H

#include <filesystem>
#include <vector>

#include "domain/model/DestinationEntry.h"
#include "domain/ports/FileOperations.h"
#include "domain/ports/LinkService.h"

[[nodiscard]] std::vector<std::filesystem::path> EnabledAddonFolders(
    const std::vector<DestinationEntry>& entries);

class EnabledStateResolver
{
public:
    EnabledStateResolver(const LinkService& linkService, const FileOperations& fileOperations);

    [[nodiscard]] std::vector<DestinationEntry> Resolve(
        const std::vector<std::filesystem::path>& destinationRoots,
        const std::vector<std::filesystem::path>& libraryRoots) const;

private:
    [[nodiscard]] DestinationEntry ClassifyEntry(
        const std::filesystem::path& entryPath,
        const std::vector<std::filesystem::path>& libraryRoots) const;

    const LinkService& linkService_;
    const FileOperations& fileOperations_;
};

#endif // FS_ORGANIZER_DOMAIN_LINKING_ENABLED_STATE_RESOLVER_H
