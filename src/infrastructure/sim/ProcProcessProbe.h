#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_PROC_PROCESS_PROBE_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_PROC_PROCESS_PROBE_H

#include <filesystem>
#include <string>
#include <vector>

#include "application/ports/ProcessProbe.h"

class ProcProcessProbe final : public ProcessProbe
{
public:
    explicit ProcProcessProbe(std::vector<std::string> executableNames, std::filesystem::path procFolder = "/proc");

    [[nodiscard]] std::optional<std::string> RunningSimulator() const override;

private:
    std::vector<std::string> executableNames_;
    std::filesystem::path procFolder_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_PROC_PROCESS_PROBE_H
