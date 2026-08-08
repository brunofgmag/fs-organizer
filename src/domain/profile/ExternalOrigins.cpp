#include "domain/profile/ExternalOrigins.h"

#include <algorithm>
#include <string>

#include "domain/support/PathUtils.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    const Library* LibraryWithId(const SimulatorProfile& profile, const LibraryId& libraryId)
    {
        const auto match = std::ranges::find_if(profile.libraries,
                                                [&libraryId](const Library& library)
                                                {
                                                    return library.id == libraryId;
                                                });

        return match == profile.libraries.end() ? nullptr : &*match;
    }

    std::vector<ExternalOrigin>::const_iterator Recorded(const SimulatorProfile& profile,
                                                         const std::filesystem::path& addonFolder)
    {
        const std::string wanted = ComparablePath(addonFolder);

        return std::ranges::find_if(profile.externalOrigins,
                                    [&profile, &wanted](const ExternalOrigin& origin)
                                    {
                                        const Library* library = LibraryWithId(profile, origin.libraryId);

                                        return library != nullptr
                                            && ComparablePath(PathUnder(library->path, origin.relativePath)) == wanted;
                                    });
    }
}

std::vector<ExternalAddon> ExternalAddonsOf(const SimulatorProfile& profile)
{
    std::vector<ExternalAddon> externals;

    for (const ExternalOrigin& origin : profile.externalOrigins)
    {
        const Library* library = LibraryWithId(profile, origin.libraryId);
        if (library == nullptr || origin.relativePath.empty() || origin.externalPath.empty())
        {
            continue;
        }

        externals.push_back(ExternalAddon{.addonFolder = PathUnder(library->path, origin.relativePath),
                                          .externalPath = origin.externalPath});
    }

    return externals;
}

std::filesystem::path ExternalOriginOf(const std::vector<ExternalAddon>& externals,
                                       const std::filesystem::path& addonFolder)
{
    const std::string wanted = ComparablePath(addonFolder);

    const auto match = std::ranges::find_if(externals,
                                            [&wanted](const ExternalAddon& external)
                                            {
                                                return ComparablePath(external.addonFolder) == wanted;
                                            });

    return match == externals.end() ? std::filesystem::path{} : match->externalPath;
}

std::filesystem::path LibraryCopyOf(const std::vector<ExternalAddon>& externals,
                                    const std::filesystem::path& externalPath)
{
    const std::string wanted = ComparablePath(externalPath);

    const auto match = std::ranges::find_if(externals,
                                            [&wanted](const ExternalAddon& external)
                                            {
                                                return ComparablePath(external.externalPath) == wanted;
                                            });

    return match == externals.end() ? std::filesystem::path{} : match->addonFolder;
}

void RememberedByTheLibrary(std::vector<ExternalAddon>& externals,
                            const std::filesystem::path& addonFolder,
                            const std::filesystem::path& externalPath)
{
    const std::string wanted = ComparablePath(addonFolder);

    const auto known = std::ranges::find_if(externals,
                                            [&wanted](const ExternalAddon& external)
                                            {
                                                return ComparablePath(external.addonFolder) == wanted;
                                            });

    if (known != externals.end())
    {
        known->externalPath = externalPath;
        return;
    }

    externals.push_back(ExternalAddon{.addonFolder = addonFolder, .externalPath = externalPath});
}

void RememberWhereItCameFrom(SimulatorProfile& profile,
                             const std::filesystem::path& addonFolder,
                             const std::filesystem::path& externalPath)
{
    const Library* library = LibraryContaining(profile, addonFolder);
    if (library == nullptr)
    {
        return;
    }

    ForgetWhereItCameFrom(profile, addonFolder);

    profile.externalOrigins.push_back(ExternalOrigin{.libraryId = library->id,
                                                     .relativePath = addonFolder.lexically_relative(library->path),
                                                     .externalPath = externalPath});
}

void ForgetWhereItCameFrom(SimulatorProfile& profile, const std::filesystem::path& addonFolder)
{
    for (auto recorded = Recorded(profile, addonFolder); recorded != profile.externalOrigins.end();
         recorded = Recorded(profile, addonFolder))
    {
        profile.externalOrigins.erase(recorded);
    }
}
