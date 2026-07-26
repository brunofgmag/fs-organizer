#ifndef FS_ORGANIZER_DOMAIN_PORTS_PROCESS_PROBE_H
#define FS_ORGANIZER_DOMAIN_PORTS_PROCESS_PROBE_H

#include <optional>
#include <string>

class ProcessProbe
{
public:
    virtual ~ProcessProbe() = default;

    [[nodiscard]] virtual std::optional<std::string> RunningSimulator() const = 0;

    [[nodiscard]] bool SimulatorIsRunning() const
    {
        return RunningSimulator().has_value();
    }
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_PROCESS_PROBE_H
