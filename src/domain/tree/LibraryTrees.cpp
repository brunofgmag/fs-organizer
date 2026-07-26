#include "domain/tree/LibraryTrees.h"

std::vector<TreeNode> LibraryTreesOf(const CatalogScanner& catalog, const SimulatorProfile& profile)
{
    std::vector<TreeNode> libraries;
    libraries.reserve(profile.libraries.size());

    for (const Library& library : profile.libraries)
    {
        libraries.push_back(catalog.Scan(library.path));
    }

    return libraries;
}
