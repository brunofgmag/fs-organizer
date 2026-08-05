#ifndef FS_ORGANIZER_INFRASTRUCTURE_CATALOG_FILESYSTEM_SCANNER_H
#define FS_ORGANIZER_INFRASTRUCTURE_CATALOG_FILESYSTEM_SCANNER_H

#include "domain/ports/CatalogScanner.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/ManifestParser.h"

class FilesystemScanner final : public CatalogScanner
{
public:
    FilesystemScanner(const ManifestParser& manifestParser, const FilesystemProbe& filesystemProbe);

    [[nodiscard]] TreeNode Scan(const std::filesystem::path& libraryRoot) const override;

private:
    [[nodiscard]] bool HasManifest(const std::filesystem::path& folder) const;

    [[nodiscard]] bool WasDeclaredACategory(const std::filesystem::path& folder) const;

    [[nodiscard]] TreeNode ScanFolder(const std::filesystem::path& folder) const;

    [[nodiscard]] TreeNode ScanAddon(const std::filesystem::path& folder) const;

    [[nodiscard]] TreeNode ScanCategory(const std::filesystem::path& folder) const;

    const ManifestParser& manifestParser_;
    const FilesystemProbe& filesystemProbe_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_CATALOG_FILESYSTEM_SCANNER_H
