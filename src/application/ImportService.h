#ifndef FS_ORGANIZER_APPLICATION_IMPORT_SERVICE_H
#define FS_ORGANIZER_APPLICATION_IMPORT_SERVICE_H

#include <cstdint>
#include <functional>
#include <vector>

#include "application/model/ConflictDetails.h"
#include "application/model/FileOperationResult.h"
#include "application/model/ImportOperationResult.h"
#include "application/model/QuarantinedItem.h"
#include "application/model/StagingLeftover.h"
#include "application/ports/ProcessProbe.h"
#include "domain/importing/ImportEngine.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/model/ConflictChoice.h"
#include "domain/model/CopyConflict.h"
#include "domain/ports/CatalogScanner.h"
#include "domain/ports/Clock.h"
#include "domain/ports/OperationJournal.h"

class ImportService
{
public:
    ImportService(const ImportEngine& engine,
                  const ProcessProbe& processProbe,
                  const FilesystemProbe& filesystemProbe,
                  const CatalogScanner& catalog,
                  FileOperations& files,
                  const LinkingEngine& linking,
                  const OperationLog& log,
                  LinkType linkType);

    void UseLinkType(LinkType linkType);

    [[nodiscard]] std::vector<ImportOperationResult>
    Import(const SimulatorProfile& profile,
           const std::vector<ImportRequest>& requests,
           const std::function<bool(const CopyProgress&)>& onProgress,
           const std::function<void(OperationKind)>& onStep = {}) const;

    [[nodiscard]] FileResult ResolveConflict(const SimulatorProfile& profile,
                                             const std::vector<DestinationEntry>& entries,
                                             const CopyConflict& conflict,
                                             ConflictChoice choice) const;

    [[nodiscard]] ConflictDetails DetailsOf(const std::vector<DestinationEntry>& entries,
                                            const CopyConflict& conflict) const;

    [[nodiscard]] std::uintmax_t TotalSizeOf(const std::vector<std::filesystem::path>& folders) const;

    [[nodiscard]] std::vector<QuarantinedItem> Quarantined(const SimulatorProfile& profile) const;

    [[nodiscard]] std::vector<FileOperationResult> Restore(const SimulatorProfile& profile,
                                                           const std::vector<QuarantinedItem>& items) const;

    [[nodiscard]] std::vector<FileOperationResult> Discard(const SimulatorProfile& profile,
                                                           const std::vector<QuarantinedItem>& items) const;

    [[nodiscard]] std::vector<StagingLeftover> Leftovers(const SimulatorProfile& profile) const;

    [[nodiscard]] std::vector<ImportOperationResult>
    Resume(const SimulatorProfile& profile,
           const std::vector<StagingLeftover>& leftovers,
           const std::function<bool(const CopyProgress&)>& onProgress,
           const std::function<void(OperationKind)>& onStep = {}) const;

    [[nodiscard]] std::vector<FileOperationResult>
    DiscardLeftovers(const SimulatorProfile& profile, const std::vector<StagingLeftover>& leftovers) const;

private:
    [[nodiscard]] ConflictSide SideOf(const std::filesystem::path& folder) const;

    [[nodiscard]] FileResult RestoreOne(const SimulatorProfile& profile, const QuarantinedItem& item) const;

    [[nodiscard]] FileResult DiscardOne(const SimulatorProfile& profile, const QuarantinedItem& item) const;

    [[nodiscard]] FileResult DiscardOneStaging(const SimulatorProfile& profile, const StagingLeftover& leftover) const;

    void Record(const SimulatorProfile& profile,
                OperationKind kind,
                const std::filesystem::path& addonFolder,
                const std::filesystem::path& source,
                const std::filesystem::path& target,
                FileResult result) const;

    const ImportEngine& engine_;
    const ProcessProbe& processProbe_;
    const FilesystemProbe& filesystemProbe_;
    const CatalogScanner& catalog_;
    FileOperations& files_;
    const LinkingEngine& linking_;
    const OperationLog& log_;
    LinkType linkType_;
};

#endif // FS_ORGANIZER_APPLICATION_IMPORT_SERVICE_H
