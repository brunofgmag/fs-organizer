#ifndef FS_ORGANIZER_APPLICATION_DOCUMENT_SERVICE_H
#define FS_ORGANIZER_APPLICATION_DOCUMENT_SERVICE_H

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "application/model/AddonDocuments.h"
#include "domain/model/AddonId.h"
#include "domain/model/Library.h"
#include "domain/ports/CatalogScanner.h"
#include "domain/ports/ChartCatalogueParser.h"
#include "domain/ports/ChartVersions.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/scenery/AirportCoverage.h"

using DocumentProgress = std::function<bool(std::size_t indexed, std::size_t outOf)>;

class DocumentService
{
public:
    DocumentService(const CatalogScanner& catalog,
                    const FilesystemProbe& filesystemProbe,
                    const ChartCatalogueParser& catalogueParser,
                    const ChartVersions& chartVersions);

    [[nodiscard]] DocumentsOfAnAddon
    DocumentsOf(const AddonId& addon, const std::filesystem::path& folder, const std::vector<std::string>& codes) const;

    [[nodiscard]] std::vector<DocumentsOfAnAddon> IndexWhile(const std::vector<Library>& libraries,
                                                             const std::vector<AirportsOfAnAddon>& airports,
                                                             const DocumentProgress& onProgress) const;

private:
    [[nodiscard]] std::vector<CatalogueOfAnAirport> CataloguesBeside(const std::filesystem::path& folder,
                                                                     const std::vector<ChartFile>& charts) const;

    [[nodiscard]] std::vector<ChartVersion> TheVersionsOf(const std::vector<std::filesystem::path>& charts,
                                                          const std::filesystem::path& folder) const;

    const CatalogScanner& catalog_;
    const FilesystemProbe& filesystemProbe_;
    const ChartCatalogueParser& catalogueParser_;
    const ChartVersions& chartVersions_;
};

#endif // FS_ORGANIZER_APPLICATION_DOCUMENT_SERVICE_H
