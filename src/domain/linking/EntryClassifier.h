#ifndef FS_ORGANIZER_DOMAIN_LINKING_ENTRY_CLASSIFIER_H
#define FS_ORGANIZER_DOMAIN_LINKING_ENTRY_CLASSIFIER_H

#include <filesystem>
#include <vector>

#include "domain/model/DestinationEntry.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/LinkService.h"

[[nodiscard]] std::vector<std::filesystem::path> EnabledAddonFolders(
    const std::vector<DestinationEntry>& entries);

[[nodiscard]] std::vector<std::filesystem::path> LinksPointingAt(
    const std::vector<DestinationEntry>& entries, const std::filesystem::path& addonFolder);

class EntryClassifier
{
public:
    EntryClassifier(const LinkService& linkService, const FilesystemProbe& filesystemProbe);

    [[nodiscard]] std::vector<DestinationEntry> Resolve(
        const std::vector<std::filesystem::path>& destinationRoots,
        const std::vector<std::filesystem::path>& libraryRoots) const;

private:
    [[nodiscard]] DestinationEntry ClassifyEntry(
        const std::filesystem::path& entryPath,
        const std::vector<std::filesystem::path>& libraryRoots) const;

    const LinkService& linkService_;
    const FilesystemProbe& filesystemProbe_;
};

#endif // FS_ORGANIZER_DOMAIN_LINKING_ENTRY_CLASSIFIER_H
