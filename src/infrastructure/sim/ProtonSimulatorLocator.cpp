#include "infrastructure/sim/ProtonSimulatorLocator.h"

#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "infrastructure/sim/ChildNamedIgnoringCase.h"
#include "infrastructure/sim/DestinationFolders.h"
#include "infrastructure/sim/UserCfgPackagesPath.h"

ProtonSimulatorLocator::ProtonSimulatorLocator(std::vector<UserCfgLocation> locations, const PathMapper& pathMapper)
    : locations_(std::move(locations)), pathMapper_(pathMapper)
{
}

std::vector<SimulatorCandidate> ProtonSimulatorLocator::Locate() const
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
        candidate.packagesPath = pathMapper_.ToHost(location.variant, *packagesPath);

        std::error_code error;
        for (const std::string_view folder : kDestinationFolders)
        {
            const std::optional<std::filesystem::path> destination =
                ChildNamedIgnoringCase(candidate.packagesPath, std::string(folder));

            if (destination.has_value() && std::filesystem::is_directory(*destination, error))
            {
                candidate.destinations.push_back(*destination);
            }
        }

        candidates.push_back(candidate);
    }

    return candidates;
}
