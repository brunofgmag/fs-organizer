#ifndef FS_ORGANIZER_DOMAIN_PORTS_SIMULATOR_LOCATOR_H
#define FS_ORGANIZER_DOMAIN_PORTS_SIMULATOR_LOCATOR_H

#include <vector>

#include "domain/model/SimulatorProfile.h"

class SimulatorLocator
{
public:
    virtual ~SimulatorLocator() = default;

    [[nodiscard]] virtual std::vector<SimulatorCandidate> Locate() const = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_SIMULATOR_LOCATOR_H
