#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_PROCESS_PROBE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_PROCESS_PROBE_H

#include "domain/ports/ProcessProbe.h"

class FakeProcessProbe final : public ProcessProbe
{
public:
    void ReportTheSimulatorAsRunning()
    {
        running_ = "FlightSimulator2024.exe";
    }

    [[nodiscard]] std::optional<std::string> RunningSimulator() const override
    {
        return running_;
    }

private:
    std::optional<std::string> running_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_PROCESS_PROBE_H
