#ifndef FS_ORGANIZER_DOMAIN_PORTS_CATALOG_SCANNER_H
#define FS_ORGANIZER_DOMAIN_PORTS_CATALOG_SCANNER_H

#include <filesystem>

#include "domain/model/TreeNode.h"

class CatalogScanner
{
public:
    virtual ~CatalogScanner() = default;

    [[nodiscard]] virtual TreeNode Scan(const std::filesystem::path& libraryRoot) const = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_CATALOG_SCANNER_H
