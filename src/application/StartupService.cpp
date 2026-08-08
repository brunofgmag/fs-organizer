#include "application/StartupService.h"

StartupService::StartupService(StartupEntries& entries, const ProcessProbe& processProbe)
    : entries_(entries), processProbe_(processProbe)
{
}

std::vector<StartupEntry> StartupService::Entries() const
{
    return entries_.Entries();
}

FileResult StartupService::Switch(const std::filesystem::path& entryPath, const bool enabled)
{
    if (processProbe_.SimulatorIsRunning())
    {
        return FileResult::TheSimulatorIsRunning;
    }

    return entries_.Switch(entryPath, enabled);
}
