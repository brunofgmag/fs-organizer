#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_WINDOWS_PROCESS_PROBE_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_WINDOWS_PROCESS_PROBE_H

#include <string>
#include <vector>

#include "domain/ports/ProcessProbe.h"

class WindowsProcessProbe final : public ProcessProbe
{
public:
    explicit WindowsProcessProbe(std::vector<std::string> executableNames);

    [[nodiscard]] bool SimulatorIsRunning() const override;

private:
    std::vector<std::string> executableNames_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_WINDOWS_PROCESS_PROBE_H
