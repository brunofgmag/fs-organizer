#include "domain/tree/LibraryTrees.h"

std::vector<TreeNode>
LibraryTreesOf(const CatalogScanner& catalog, const SimulatorProfile& profile, const ScanGate& gate)
{
    std::vector<TreeNode> libraries;
    libraries.reserve(profile.libraries.size());

    for (const Library& library : profile.libraries)
    {
        if (!gate.StillWanted())
        {
            break;
        }

        libraries.push_back(catalog.ScanWhile(library.path, gate));
    }

    return libraries;
}
