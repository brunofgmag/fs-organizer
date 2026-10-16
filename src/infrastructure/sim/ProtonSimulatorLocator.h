#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_SIMULATOR_LOCATOR_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_SIMULATOR_LOCATOR_H

#include <vector>

#include "application/ports/PathMapper.h"
#include "application/ports/SimulatorLocator.h"
#include "infrastructure/sim/UserCfgLocation.h"

class ProtonSimulatorLocator final : public SimulatorLocator
{
public:
    ProtonSimulatorLocator(std::vector<UserCfgLocation> locations, const PathMapper& pathMapper);

    [[nodiscard]] std::vector<SimulatorCandidate> Locate() const override;

private:
    std::vector<UserCfgLocation> locations_;
    const PathMapper& pathMapper_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_SIMULATOR_LOCATOR_H
