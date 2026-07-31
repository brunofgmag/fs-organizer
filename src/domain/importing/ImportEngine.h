#ifndef FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_ENGINE_H
#define FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_ENGINE_H

#include <functional>

#include "domain/importing/ImportPaths.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/LinkingEngine.h"
#include "domain/model/CopyOutcome.h"
#include "domain/model/ImportOutcome.h"
#include "domain/model/ImportRequest.h"
#include "domain/model/LinkType.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/ports/Clock.h"
#include "domain/ports/FileOperations.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/OperationJournal.h"

inline constexpr std::uintmax_t kFreeSpaceMargin = 64ULL * 1024 * 1024;

class ImportEngine
{
public:
    ImportEngine(const FilesystemProbe& filesystemProbe,
                 FileOperations& files,
                 const LinkingEngine& linking,
                 const OperationLog& log,
                 LinkType linkType);

    void UseLinkType(LinkType linkType);

    [[nodiscard]] ImportOutcome Import(const SimulatorProfile& profile,
                                       const ImportRequest& request,
                                       const std::function<bool(const CopyProgress&)>& onProgress,
                                       const std::function<void(OperationKind)>& onStep = {}) const;

private:
    [[nodiscard]] ImportOutcome CheckFreeSpace(const std::filesystem::path& category, std::uintmax_t sourceSize) const;

    [[nodiscard]] ImportOutcome CopyToStaging(const std::filesystem::path& source,
                                              const std::filesystem::path& staging,
                                              const std::function<bool(const CopyProgress&)>& onProgress) const;

    void RecordStep(const AddonId& addon,
                    OperationKind kind,
                    const std::filesystem::path& source,
                    const std::filesystem::path& target,
                    FileResult result) const;

    const FilesystemProbe& filesystemProbe_;
    FileOperations& files_;
    const LinkingEngine& linking_;
    const OperationLog& log_;
    LinkType linkType_;
};

#endif // FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_ENGINE_H
