#include "domain/tree/StructureAdoption.h"

#include <set>
#include <string>

#include "domain/importing/ImportPaths.h"
#include "domain/model/CategoryMarker.h"
#include "domain/model/Manifest.h"
#include "domain/support/PathUtils.h"

namespace
{
    void CollectBelow(const FilesystemProbe& filesystemProbe,
                      const std::filesystem::path& folder,
                      const std::set<std::string>& theAppBroughtIn,
                      LibraryGrouping& grouping)
    {
        for (const std::filesystem::path& child : filesystemProbe.ChildDirectories(folder))
        {
            if (CreatedByTheImporter(child) || filesystemProbe.IsReparsePoint(child))
            {
                continue;
            }

            if (theAppBroughtIn.contains(ComparablePath(child)))
            {
                continue;
            }

            if (filesystemProbe.EntryExistsWithoutFollowingLinks(ManifestPathIn(child)))
            {
                continue;
            }

            if (filesystemProbe.EntryExistsWithoutFollowingLinks(CategoryMarkerPathIn(child)))
            {
                grouping.alreadyDeclared.push_back(child);
            }
            else
            {
                grouping.notYetDeclared.push_back(child);
            }

            CollectBelow(filesystemProbe, child, theAppBroughtIn, grouping);
        }
    }
}

LibraryGrouping HowTheLibraryIsGrouped(const FilesystemProbe& filesystemProbe,
                                       const std::filesystem::path& libraryRoot,
                                       const std::vector<std::filesystem::path>& theAppBroughtIn)
{
    std::set<std::string> brought;
    for (const std::filesystem::path& folder : theAppBroughtIn)
    {
        brought.insert(ComparablePath(folder));
    }

    LibraryGrouping grouping;
    CollectBelow(filesystemProbe, libraryRoot, brought, grouping);

    return grouping;
}
