#include "infrastructure/sim/ProtonPrefixes.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <system_error>

#include "infrastructure/platform/LinuxKnownFolders.h"
#include "infrastructure/sim/SteamLibraryFolders.h"

namespace
{
    constexpr auto kSteamApps = "steamapps";
    constexpr auto kLibraryFoldersFile = "libraryfolders.vdf";
    constexpr auto kCompatData = "compatdata";
    constexpr auto kPrefixFolder = "pfx";

    struct SimulatorOnSteam
    {
        SimulatorVariant variant = SimulatorVariant::MSFS2024;
        std::string_view appId{};
    };

    constexpr SimulatorOnSteam kSimulators[] = {
        {.variant = SimulatorVariant::MSFS2020, .appId = "1250410"},
        {.variant = SimulatorVariant::MSFS2024, .appId = "2537590"},
    };

    bool IsAlreadyIn(const std::vector<std::filesystem::path>& known, const std::filesystem::path& candidate)
    {
        std::error_code error;
        const std::filesystem::path resolved = std::filesystem::weakly_canonical(candidate, error);

        return std::ranges::any_of(known,
                                   [&resolved](const std::filesystem::path& other)
                                   {
                                       std::error_code ignored;

                                       return std::filesystem::weakly_canonical(other, ignored) == resolved;
                                   });
    }

    void Add(std::vector<std::filesystem::path>& folders, const std::filesystem::path& candidate)
    {
        if (!candidate.empty() && !IsAlreadyIn(folders, candidate))
        {
            folders.push_back(candidate);
        }
    }

    std::filesystem::path ManifestOf(const std::filesystem::path& library, std::string_view appId)
    {
        return library / kSteamApps / (std::string("appmanifest_").append(appId).append(".acf"));
    }

    std::filesystem::path PrefixOf(const std::filesystem::path& library, std::string_view appId)
    {
        return library / kSteamApps / kCompatData / appId / kPrefixFolder;
    }
}

std::vector<std::filesystem::path> SteamRoots()
{
    std::vector<std::filesystem::path> roots;

    Add(roots, DataHomeFolder() / "Steam");
    Add(roots, HomeFolder() / ".steam" / "steam");

    return roots;
}

std::vector<std::filesystem::path> SteamLibraries(const std::vector<std::filesystem::path>& roots)
{
    std::vector<std::filesystem::path> libraries;

    for (const std::filesystem::path& root : roots)
    {
        Add(libraries, root);

        for (const std::filesystem::path& library : SteamLibraryFoldersIn(root / kSteamApps / kLibraryFoldersFile))
        {
            Add(libraries, library);
        }
    }

    return libraries;
}

std::vector<ProtonPrefix> ProtonPrefixes(const std::vector<std::filesystem::path>& libraries)
{
    std::vector<ProtonPrefix> prefixes;
    std::error_code error;

    for (const std::filesystem::path& library : libraries)
    {
        for (const SimulatorOnSteam& simulator : kSimulators)
        {
            const std::filesystem::path prefix = PrefixOf(library, simulator.appId);

            if (std::filesystem::is_regular_file(ManifestOf(library, simulator.appId), error)
                && std::filesystem::is_directory(prefix, error))
            {
                prefixes.push_back({.variant = simulator.variant, .path = prefix});
            }
        }
    }

    return prefixes;
}
