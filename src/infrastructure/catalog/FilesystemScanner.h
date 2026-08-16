#ifndef FS_ORGANIZER_INFRASTRUCTURE_CATALOG_FILESYSTEM_SCANNER_H
#define FS_ORGANIZER_INFRASTRUCTURE_CATALOG_FILESYSTEM_SCANNER_H

#include <set>
#include <string>

#include "domain/ports/CatalogScanner.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/ImportedFolders.h"
#include "domain/ports/ManifestParser.h"

class FilesystemScanner final : public CatalogScanner
{
public:
    FilesystemScanner(const ManifestParser& manifestParser,
                      const FilesystemProbe& filesystemProbe,
                      const ImportedFolders& importedFolders);

    [[nodiscard]] TreeNode ScanWhile(const std::filesystem::path& libraryRoot, const ScanGate& gate) const override;

private:
    [[nodiscard]] bool HasManifest(const std::filesystem::path& folder) const;

    [[nodiscard]] bool WasDeclaredACategory(const std::filesystem::path& folder) const;

    [[nodiscard]] TreeNode
    ScanFolder(const std::filesystem::path& folder, const std::set<std::string>& brought, const ScanGate& gate) const;

    [[nodiscard]] TreeNode ScanAddon(const std::filesystem::path& folder) const;

    [[nodiscard]] TreeNode
    ScanCategory(const std::filesystem::path& folder, const std::set<std::string>& brought, const ScanGate& gate) const;

    const ManifestParser& manifestParser_;
    const FilesystemProbe& filesystemProbe_;
    const ImportedFolders& importedFolders_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_CATALOG_FILESYSTEM_SCANNER_H
