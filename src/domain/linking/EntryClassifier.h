#ifndef FS_ORGANIZER_DOMAIN_LINKING_ENTRY_CLASSIFIER_H
#define FS_ORGANIZER_DOMAIN_LINKING_ENTRY_CLASSIFIER_H

#include <filesystem>
#include <map>
#include <optional>
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

class TheAppLinkedPlaces
{
public:
    explicit TheAppLinkedPlaces(const LinkedFolders& linkedFolders);

    [[nodiscard]] const std::map<std::string, LinkTheAppMade>& Get() const;

private:
    const LinkedFolders& linkedFolders_;
    mutable std::optional<std::map<std::string, LinkTheAppMade>> places_{};
};

class ClassificationLookups
{
public:
    ClassificationLookups(const FilesystemProbe& filesystemProbe,
                          const std::vector<std::filesystem::path>& libraryRoots,
                          const std::vector<ExternalAddon>& externals);

    [[nodiscard]] std::filesystem::path ExternalOrigin(const std::string& comparableAddonFolder) const;

    [[nodiscard]] std::filesystem::path LibraryCopy(const std::string& comparableExternalPath) const;

    [[nodiscard]] bool IsInsideALibrary(const std::string& comparablePath) const;

    [[nodiscard]] bool VolumeIsAvailable(const std::filesystem::path& path) const;

private:
    const FilesystemProbe& filesystemProbe_;
    std::vector<std::string> libraryRoots_{};
    std::map<std::string, std::filesystem::path> originsByAddonFolder_{};
    std::map<std::string, std::filesystem::path> copiesByExternalPath_{};
    mutable std::map<std::string, bool> volumes_{};
};

class EntryClassifier
{
public:
    EntryClassifier(const LinkService& linkService,
                    const FilesystemProbe& filesystemProbe,
                    const LinkedFolders& linkedFolders = NoLinkWasEverMade());

    [[nodiscard]] std::vector<DestinationEntry> Resolve(const std::vector<std::filesystem::path>& destinationRoots,
                                                        const std::vector<std::filesystem::path>& libraryRoots,
                                                        const std::vector<ExternalAddon>& externals = {}) const;

    [[nodiscard]] std::vector<DestinationEntry> Refresh(const std::vector<DestinationEntry>& known,
                                                        const std::vector<std::filesystem::path>& changed,
                                                        const std::vector<std::filesystem::path>& destinationRoots,
                                                        const std::vector<std::filesystem::path>& libraryRoots,
                                                        const std::vector<ExternalAddon>& externals = {}) const;

    [[nodiscard]] std::vector<DestinationEntry> LinksAt(const std::vector<std::filesystem::path>& places,
                                                        const std::vector<std::filesystem::path>& libraryRoots,
                                                        const std::vector<ExternalAddon>& externals = {}) const;

private:
    [[nodiscard]] DestinationEntry ClassifyEntry(const std::filesystem::path& entryPath,
                                                 const std::optional<std::filesystem::path>& target,
                                                 const ClassificationLookups& lookups,
                                                 const TheAppLinkedPlaces& theAppLinked) const;

    [[nodiscard]] DestinationEntry ClassifyLink(const std::filesystem::path& entryPath,
                                                const std::filesystem::path& target,
                                                const ClassificationLookups& lookups) const;

    [[nodiscard]] DestinationEntry WhatStandsWhereALinkWas(const std::filesystem::path& entryPath,
                                                           const TheAppLinkedPlaces& theAppLinked) const;

    [[nodiscard]] bool APhysicalFolderIsThere(const std::filesystem::path& path) const;

    [[nodiscard]] bool BothCopiesAreThere(const std::filesystem::path& theOtherPrograms,
                                          const std::filesystem::path& inTheLibrary) const;

    const LinkService& linkService_;
    const FilesystemProbe& filesystemProbe_;
    const LinkedFolders& linkedFolders_;
};

#endif // FS_ORGANIZER_DOMAIN_LINKING_ENTRY_CLASSIFIER_H
