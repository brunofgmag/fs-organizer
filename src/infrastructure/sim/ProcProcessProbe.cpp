#include "infrastructure/sim/ProcProcessProbe.h"

#include <utility>

ProcProcessProbe::ProcProcessProbe(std::vector<std::string> executableNames)
    : executableNames_(std::move(executableNames))
{
}

std::optional<std::string> ProcProcessProbe::RunningSimulator() const
{
    if (executableNames_.empty())
    {
        return std::nullopt;
    }

    return executableNames_.front();
}
