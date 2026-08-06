#include "domain/tree/LibraryLookup.h"

#include <algorithm>
#include <ranges>
#include <string>

#include "domain/support/PathUtils.h"

const Library* LibraryContaining(const std::vector<Library>& libraries, const std::filesystem::path& path)
{
    const auto match = std::ranges::find_if(libraries,
                                            [&path](const Library& library)
                                            {
                                                return PathIsInside(path, library.path);
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

AddonId IdentityOf(const SimulatorProfile& profile, const std::filesystem::path& addonFolder)
{
    const Library* library = LibraryContaining(profile, addonFolder);

    return AddonId{.libraryId = library == nullptr ? LibraryId{} : library->id,
                   .folderName = AsUtf8(addonFolder.filename())};
}
