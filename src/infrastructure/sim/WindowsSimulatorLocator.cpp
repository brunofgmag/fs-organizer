#include "infrastructure/sim/WindowsSimulatorLocator.h"

#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

#include "infrastructure/sim/DestinationFolders.h"
#include "infrastructure/sim/UserCfgPackagesPath.h"

WindowsSimulatorLocator::WindowsSimulatorLocator(std::vector<UserCfgLocation> locations)
    : locations_(std::move(locations))
{
}

std::vector<SimulatorCandidate> WindowsSimulatorLocator::Locate() const
{
    std::vector<SimulatorCandidate> candidates;

    for (const UserCfgLocation& location : locations_)
    {
        const std::optional<std::filesystem::path> packagesPath = InstalledPackagesPathIn(location.configPath);
        if (!packagesPath.has_value())
        {
            continue;
        }

        SimulatorCandidate candidate;
        candidate.variant = location.variant;
        candidate.packagesPath = *packagesPath;

        std::error_code error;
        for (const std::string_view folder : kDestinationFolders)
        {
            const std::filesystem::path destination = *packagesPath / folder;
            if (std::filesystem::is_directory(destination, error))
            {
                candidate.destinations.push_back(destination);
            }
        }

        candidates.push_back(candidate);
    }

    return candidates;
}
