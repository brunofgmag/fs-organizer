#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_PROCESS_PROBE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_PROCESS_PROBE_H

#include "domain/ports/ProcessProbe.h"

class FakeProcessProbe final : public ProcessProbe
{
public:
    void ReportTheSimulatorAsRunning()
    {
        running_ = true;
    }

    [[nodiscard]] bool SimulatorIsRunning() const override
    {
        return running_;
    }

private:
    bool running_ = false;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_PROCESS_PROBE_H
