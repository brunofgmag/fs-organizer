#ifndef FS_ORGANIZER_APPLICATION_LIBRARY_ORGANIZER_H
#define FS_ORGANIZER_APPLICATION_LIBRARY_ORGANIZER_H

#include <string>
#include <vector>

#include "application/model/AddonMove.h"
#include "application/model/FileOperationResult.h"
#include "application/ports/ProcessProbe.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/linking/LinkingEngine.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/ports/CatalogScanner.h"
#include "domain/ports/Clock.h"
#include "domain/ports/FileOperations.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/OperationJournal.h"

class LibraryOrganizer
{
public:
    LibraryOrganizer(const CatalogScanner& catalog,
                     const FilesystemProbe& filesystemProbe,
                     FileOperations& files,
                     const LinkingEngine& linking,
                     const EntryClassifier& classifier,
                     const ProcessProbe& processProbe,
                     const OperationLog& log,
                     LinkType linkType);

    void UseLinkType(LinkType linkType);

    [[nodiscard]] FileOperationResult
    CreateCategory(const SimulatorProfile& profile, const std::filesystem::path& parent, const std::string& name) const;

    [[nodiscard]] FileOperationResult
    RenameCategory(SimulatorProfile& profile, const std::filesystem::path& category, const std::string& name) const;

    [[nodiscard]] FileOperationResult RemoveCategory(SimulatorProfile& profile,
                                                     const std::filesystem::path& category) const;

    [[nodiscard]] std::vector<FileOperationResult> Move(SimulatorProfile& profile,
                                                        const std::vector<AddonMove>& moves) const;

private:
    [[nodiscard]] FileOperationResult
    MoveOne(SimulatorProfile& profile, const std::vector<TreeNode>& libraries, const AddonMove& move) const;

    [[nodiscard]] bool
    Relink(const SimulatorProfile& profile, const AddonId& addon, const std::filesystem::path& folder) const;

    void DeclareACategory(const Library& library, const std::filesystem::path& folder) const;

    void Record(OperationKind kind,
                const AddonId& addon,
                const std::filesystem::path& source,
                const std::filesystem::path& target,
                FileResult result) const;

    const CatalogScanner& catalog_;
    const FilesystemProbe& filesystemProbe_;
    FileOperations& files_;
    const LinkingEngine& linking_;
    const EntryClassifier& classifier_;
    const ProcessProbe& processProbe_;
    const OperationLog& log_;
    LinkType linkType_;
};

#endif // FS_ORGANIZER_APPLICATION_LIBRARY_ORGANIZER_H
