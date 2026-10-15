#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_SIMULATOR_LOCATOR_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_SIMULATOR_LOCATOR_H

#include <vector>

#include "application/ports/SimulatorLocator.h"
#include "infrastructure/sim/UserCfgLocation.h"

class ProtonSimulatorLocator final : public SimulatorLocator
{
public:
    explicit ProtonSimulatorLocator(std::vector<UserCfgLocation> locations);

    [[nodiscard]] std::vector<SimulatorCandidate> Locate() const override;

private:
    std::vector<UserCfgLocation> locations_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_SIMULATOR_LOCATOR_H
