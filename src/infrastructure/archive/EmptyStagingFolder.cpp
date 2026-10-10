#include "infrastructure/archive/EmptyStagingFolder.h"

#include <system_error>

#include "domain/importing/ImportPaths.h"
#include "infrastructure/fileops/ExtendedPaths.h"

bool IsAnEmptyStagingFolder(const std::filesystem::path& folder)
{
    std::error_code error;
    const std::filesystem::path reachable = WithExtendedPrefix(folder);

    return std::filesystem::is_directory(reachable, error) && std::filesystem::is_empty(reachable, error)
        && IsStagingPath(folder);
}
