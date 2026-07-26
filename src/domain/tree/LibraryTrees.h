#ifndef FS_ORGANIZER_DOMAIN_TREE_LIBRARY_TREES_H
#define FS_ORGANIZER_DOMAIN_TREE_LIBRARY_TREES_H

#include <vector>

#include "domain/model/SimulatorProfile.h"
#include "domain/model/TreeNode.h"
#include "domain/ports/CatalogScanner.h"

[[nodiscard]] std::vector<TreeNode> LibraryTreesOf(const CatalogScanner& catalog, const SimulatorProfile& profile);

#endif // FS_ORGANIZER_DOMAIN_TREE_LIBRARY_TREES_H
