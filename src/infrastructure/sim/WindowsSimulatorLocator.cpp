#include "infrastructure/sim/WindowsSimulatorLocator.h"

#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{
    constexpr std::string_view kInstalledPackagesKey = "InstalledPackagesPath";
    constexpr std::string_view kDestinationFolders[] = {"Community", "Community2024"};

    std::optional<std::filesystem::path> ReadPackagesPath(const std::filesystem::path& configPath)
    {
        std::ifstream file(configPath, std::ios::binary);

        std::string line;
        while (std::getline(file, line))
        {
            const std::size_t key = line.find(kInstalledPackagesKey);
            if (key == std::string::npos)
            {
                continue;
            }

            const std::size_t opening = line.find('"', key + kInstalledPackagesKey.size());
            if (opening == std::string::npos)
            {
                continue;
            }

            const std::size_t closing = line.find('"', opening + 1);
            if (closing == std::string::npos)
            {
                continue;
            }

            return std::filesystem::path(line.substr(opening + 1, closing - opening - 1));
        }

        return std::nullopt;
    }
}

WindowsSimulatorLocator::WindowsSimulatorLocator(std::vector<UserCfgLocation> locations)
    : locations_(std::move(locations))
{
}

std::vector<SimulatorCandidate> WindowsSimulatorLocator::Locate() const
{
    std::vector<SimulatorCandidate> candidates;

    for (const UserCfgLocation& location : locations_)
    {
        const std::optional<std::filesystem::path> packagesPath = ReadPackagesPath(location.configPath);
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
