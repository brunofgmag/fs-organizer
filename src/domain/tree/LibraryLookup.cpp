#include "domain/tree/LibraryLookup.h"

#include <algorithm>
#include <ranges>
#include <string>

#include "domain/support/PathUtils.h"

namespace
{
    bool IsInside(const std::string& path, const std::string& root)
    {
        return path == root || (path.size() > root.size() && path.compare(0, root.size(), root) == 0
            && path[root.size()] == '/');
    }
}

const Library* LibraryContaining(const std::vector<Library>& libraries, const std::filesystem::path& path)
{
    const std::string candidate = ComparablePath(path);

    const auto match = std::ranges::find_if(libraries, [&candidate](const Library& library)
    {
        return IsInside(candidate, ComparablePath(library.path));
    });

    return match == libraries.end() ? nullptr : &*match;
}

const Library* LibraryContaining(const SimulatorProfile& profile, const std::filesystem::path& path)
{
    return LibraryContaining(profile.libraries, path);
}

std::filesystem::path RelativeToLibrary(const Library& library, const std::filesystem::path& path)
{
    const std::string folder = ComparablePath(path);
    const std::string root = ComparablePath(library.path);

    return folder.size() > root.size() ? folder.substr(root.size() + 1) : std::string{};
}
