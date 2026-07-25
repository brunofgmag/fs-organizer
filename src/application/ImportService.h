#ifndef FS_ORGANIZER_APPLICATION_IMPORT_SERVICE_H
#define FS_ORGANIZER_APPLICATION_IMPORT_SERVICE_H

#include <functional>
#include <vector>

#include "application/model/ImportOperationResult.h"
#include "domain/importing/ImportEngine.h"
#include "domain/model/ConflictChoice.h"
#include "domain/model/CopyConflict.h"
#include "domain/ports/Clock.h"
#include "domain/ports/OperationJournal.h"
#include "domain/ports/ProcessProbe.h"

inline constexpr auto kQuarantineFolderName = "_fsorganizer-quarantine";

class ImportService
{
public:
    ImportService(const ImportEngine& engine,
                  const ProcessProbe& processProbe,
                  FileOperations& files,
                  const LinkingEngine& linking,
                  OperationJournal& journal,
                  const Clock& clock,
                  LinkType linkType);

    [[nodiscard]] std::vector<ImportOperationResult> Import(
        const SimulatorProfile& profile,
        const std::vector<ImportRequest>& requests,
        const std::function<bool(const CopyProgress &)>& onProgress) const;

    [[nodiscard]] ImportResult ResolveConflict(const SimulatorProfile& profile,
                                               const CopyConflict& conflict,
                                               ConflictChoice choice) const;

private:
    const ImportEngine& engine_;
    const ProcessProbe& processProbe_;
    FileOperations& files_;
    const LinkingEngine& linking_;
    OperationJournal& journal_;
    const Clock& clock_;
    LinkType linkType_;
};

#endif // FS_ORGANIZER_APPLICATION_IMPORT_SERVICE_H
