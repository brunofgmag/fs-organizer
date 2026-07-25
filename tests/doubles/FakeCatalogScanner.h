#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_CATALOG_SCANNER_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_CATALOG_SCANNER_H

#include <map>
#include <string>
#include <utility>

#include "domain/ports/CatalogScanner.h"

class FakeCatalogScanner final : public CatalogScanner
{
public:
    void SetTree(const std::filesystem::path& root, TreeNode tree)
    {
        trees_[root.generic_string()] = std::move(tree);
    }

    [[nodiscard]] TreeNode Scan(const std::filesystem::path& root) const override
    {
        const auto found = trees_.find(root.generic_string());

        return found == trees_.end() ? TreeNode{} : found->second;
    }

private:
    std::map<std::string, TreeNode> trees_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_CATALOG_SCANNER_H
