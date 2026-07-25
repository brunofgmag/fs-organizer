#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_SIMULATOR_LOCATOR_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_SIMULATOR_LOCATOR_H

#include <utility>
#include <vector>

#include "domain/ports/SimulatorLocator.h"

class FakeSimulatorLocator final : public SimulatorLocator
{
public:
    explicit FakeSimulatorLocator(std::vector<SimulatorCandidate> candidates) : candidates_(std::move(candidates))
    {
    }

    [[nodiscard]] std::vector<SimulatorCandidate> Locate() const override
    {
        return candidates_;
    }

private:
    std::vector<SimulatorCandidate> candidates_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_SIMULATOR_LOCATOR_H
