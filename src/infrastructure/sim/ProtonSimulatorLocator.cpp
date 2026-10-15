#include "infrastructure/sim/ProtonSimulatorLocator.h"

#include <utility>

ProtonSimulatorLocator::ProtonSimulatorLocator(std::vector<UserCfgLocation> locations)
    : locations_(std::move(locations))
{
}

std::vector<SimulatorCandidate> ProtonSimulatorLocator::Locate() const
{
    return {};
}
