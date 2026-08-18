#ifndef FS_ORGANIZER_DOMAIN_LINKING_ENTRY_CLASSIFIER_H
#define FS_ORGANIZER_DOMAIN_LINKING_ENTRY_CLASSIFIER_H

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "domain/model/DestinationEntry.h"
#include "domain/model/ExternalAddon.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/LinkedFolders.h"
#include "domain/ports/LinkService.h"

[[nodiscard]] std::vector<std::filesystem::path> EnabledAddonFolders(const std::vector<DestinationEntry>& entries);

[[nodiscard]] std::vector<std::filesystem::path> LinksPointingAt(const std::vector<DestinationEntry>& entries,
                                                                 const std::filesystem::path& addonFolder);

class EntryClassifier
{
public:
    EntryClassifier(const LinkService& linkService,
                    const FilesystemProbe& filesystemProbe,
                    const LinkedFolders& linkedFolders = NoLinkWasEverMade());

    [[nodiscard]] std::vector<DestinationEntry> Resolve(const std::vector<std::filesystem::path>& destinationRoots,
                                                        const std::vector<std::filesystem::path>& libraryRoots,
                                                        const std::vector<ExternalAddon>& externals = {}) const;

private:
    [[nodiscard]] DestinationEntry ClassifyEntry(const std::filesystem::path& entryPath,
                                                 const std::vector<std::filesystem::path>& libraryRoots,
                                                 const std::vector<ExternalAddon>& externals,
                                                 const std::map<std::string, LinkTheAppMade>& theAppLinked) const;

    [[nodiscard]] DestinationEntry
    WhatStandsWhereALinkWas(const std::filesystem::path& entryPath,
                            const std::map<std::string, LinkTheAppMade>& theAppLinked) const;

    [[nodiscard]] bool APhysicalFolderIsThere(const std::filesystem::path& path) const;

    [[nodiscard]] bool BothCopiesAreThere(const std::filesystem::path& theOtherPrograms,
                                          const std::filesystem::path& inTheLibrary) const;

    const LinkService& linkService_;
    const FilesystemProbe& filesystemProbe_;
    const LinkedFolders& linkedFolders_;
};

#endif // FS_ORGANIZER_DOMAIN_LINKING_ENTRY_CLASSIFIER_H
