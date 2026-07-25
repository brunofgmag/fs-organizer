#ifndef FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_ENGINE_H
#define FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_ENGINE_H

#include <functional>

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
inline constexpr auto kStagingSuffix = ".fsorg-partial";

class ImportEngine
{
public:
    ImportEngine(const FilesystemProbe& filesystemProbe,
                 FileOperations& files,
                 const LinkingEngine& linking,
                 OperationJournal& journal,
                 const Clock& clock,
                 LinkType linkType);

    [[nodiscard]] ImportOutcome Import(const SimulatorProfile& profile,
                                       const ImportRequest& request,
                                       const std::function<bool(const CopyProgress &)>& onProgress) const;

private:
    [[nodiscard]] ImportOutcome CheckFreeSpace(const std::filesystem::path& target, std::uintmax_t sourceSize) const;

    [[nodiscard]] ImportOutcome CopyToStaging(const std::filesystem::path& source,
                                              const std::filesystem::path& staging,
                                              const std::function<bool(const CopyProgress &)>& onProgress) const;

    void RecordStep(const AddonId& addon,
                    OperationKind kind,
                    const std::filesystem::path& source,
                    const std::filesystem::path& target,
                    ImportResult result) const;

    const FilesystemProbe& filesystemProbe_;
    FileOperations& files_;
    const LinkingEngine& linking_;
    OperationJournal& journal_;
    const Clock& clock_;
    LinkType linkType_;
};

#endif // FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_ENGINE_H
