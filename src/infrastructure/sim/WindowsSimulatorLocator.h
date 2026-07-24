#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_WINDOWS_SIMULATOR_LOCATOR_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_WINDOWS_SIMULATOR_LOCATOR_H

#include <filesystem>
#include <vector>

#include "domain/ports/SimulatorLocator.h"

struct UserCfgLocation
{
    SimulatorVariant variant = SimulatorVariant::MSFS2024;
    std::filesystem::path configPath;
};

class WindowsSimulatorLocator final : public SimulatorLocator
{
public:
    explicit WindowsSimulatorLocator(std::vector<UserCfgLocation> locations);

    [[nodiscard]] std::vector<SimulatorCandidate> Locate() const override;

private:
    std::vector<UserCfgLocation> locations_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_WINDOWS_SIMULATOR_LOCATOR_H
