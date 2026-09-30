#ifndef FS_ORGANIZER_APPLICATION_DOCUMENT_SERVICE_H
#define FS_ORGANIZER_APPLICATION_DOCUMENT_SERVICE_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "application/SceneryService.h"
#include "application/model/AddonDocuments.h"
#include "domain/model/AddonId.h"
#include "domain/ports/ChartCatalogueParser.h"
#include "domain/ports/ChartVersions.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/scenery/AirportCoverage.h"

using DocumentProgress = std::function<bool(const DocumentsOfAnAddon& addon, std::size_t indexed, std::size_t outOf)>;

class DocumentService
{
public:
    static constexpr std::uint32_t kIndexingRulesVersion = 1;

    DocumentService(const FilesystemProbe& filesystemProbe,
                    const ChartCatalogueParser& catalogueParser,
                    const ChartVersions& chartVersions);

    [[nodiscard]] static std::string DigestOf(const TreeFingerprint& walk,
                                              const std::vector<std::string>& codes,
                                              std::uint32_t rulesVersion = kIndexingRulesVersion);

    [[nodiscard]] DocumentsOfAnAddon DocumentsOf(const AddonId& addon,
                                                 const std::filesystem::path& folder,
                                                 const std::vector<std::string>& codes,
                                                 const DocumentsOfAnAddon* before = nullptr) const;

    [[nodiscard]] std::vector<DocumentsOfAnAddon> IndexWhile(const std::vector<AddonToRead>& addons,
                                                             const std::vector<AirportsOfAnAddon>& airports,
                                                             const std::vector<DocumentsOfAnAddon>& before,
                                                             const DocumentProgress& onProgress) const;

private:
    [[nodiscard]] std::vector<CatalogueOfAnAirport> CataloguesBeside(const std::filesystem::path& folder,
                                                                     const std::vector<ChartFile>& charts) const;

    [[nodiscard]] std::vector<ChartVersion> TheVersionsOf(const std::vector<std::filesystem::path>& charts,
                                                          const std::filesystem::path& folder) const;

    const FilesystemProbe& filesystemProbe_;
    const ChartCatalogueParser& catalogueParser_;
    const ChartVersions& chartVersions_;
};

#endif // FS_ORGANIZER_APPLICATION_DOCUMENT_SERVICE_H
