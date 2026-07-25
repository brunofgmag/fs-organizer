#ifndef FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_ENGINE_H
#define FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_ENGINE_H

#include <functional>

#include "domain/model/CopyOutcome.h"
#include "domain/model/ImportOutcome.h"
#include "domain/model/ImportRequest.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/ports/FileOperations.h"
#include "domain/ports/FilesystemProbe.h"

inline constexpr std::uintmax_t kFreeSpaceMargin = 64ULL * 1024 * 1024;
inline constexpr auto kStagingSuffix = ".fsorg-partial";

class ImportEngine
{
public:
    ImportEngine(const FilesystemProbe& filesystemProbe, FileOperations& files);

    [[nodiscard]] ImportOutcome Import(const SimulatorProfile& profile,
                                       const ImportRequest& request,
                                       const std::function<bool(const CopyProgress&)>& onProgress) const;

private:
    [[nodiscard]] ImportOutcome CheckFreeSpace(const ImportRequest& request) const;

    [[nodiscard]] ImportOutcome CopyToStaging(const std::filesystem::path& source,
                                              const std::filesystem::path& staging,
                                              const std::function<bool(const CopyProgress&)>& onProgress) const;

    const FilesystemProbe& filesystemProbe_;
    FileOperations& files_;
};

#endif // FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_ENGINE_H
