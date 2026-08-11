#ifndef FS_ORGANIZER_DOMAIN_PORTS_CATALOG_SCANNER_H
#define FS_ORGANIZER_DOMAIN_PORTS_CATALOG_SCANNER_H

#include <filesystem>
#include <functional>

#include "domain/model/TreeNode.h"

struct ScanGate
{
    std::function<bool()> keepGoing{};

    [[nodiscard]] bool StillWanted() const
    {
        return !keepGoing || keepGoing();
    }
};

class CatalogScanner
{
public:
    virtual ~CatalogScanner() = default;

    [[nodiscard]] virtual TreeNode ScanWhile(const std::filesystem::path& libraryRoot, const ScanGate& gate) const = 0;

    [[nodiscard]] TreeNode Scan(const std::filesystem::path& libraryRoot) const
    {
        return ScanWhile(libraryRoot, {});
    }
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_CATALOG_SCANNER_H
