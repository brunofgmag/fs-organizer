#ifndef FS_ORGANIZER_DOMAIN_PORTS_PROCESS_PROBE_H
#define FS_ORGANIZER_DOMAIN_PORTS_PROCESS_PROBE_H

class ProcessProbe
{
public:
    virtual ~ProcessProbe() = default;

    [[nodiscard]] virtual bool SimulatorIsRunning() const = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_PROCESS_PROBE_H
