#include "application/StartupService.h"

#include <variant>

StartupService::StartupService(StartupEntries& entries,
                               const ProcessProbe& processProbe,
                               const FilesystemProbe& filesystemProbe,
                               const bool managing)
    : entries_(entries), processProbe_(processProbe), filesystemProbe_(filesystemProbe), managing_(managing)
{
}

void StartupService::Manage(const bool managing)
{
    managing_ = managing;
}

bool StartupService::Managing() const
{
    return managing_;
}

std::optional<std::string> StartupService::RunningSimulator() const
{
    return processProbe_.RunningSimulator();
}

std::vector<StartupEntry> StartupService::Entries() const
{
    if (!managing_)
    {
        return {};
    }

    return entries_.Entries();
}

StartupReport StartupService::Report(const SimulatorProfile& profile, const ProfileSnapshot& snapshot) const
{
    if (!managing_)
    {
        return {};
    }

    return ReportStartupEntries(entries_.Entries(), profile, snapshot, filesystemProbe_);
}

FileResult StartupService::Switch(const std::filesystem::path& entryPath, const bool enabled)
{
    StartupBackup alone;

    return Switch(entryPath, enabled, alone);
}

FileResult StartupService::Switch(const std::filesystem::path& entryPath, const bool enabled, StartupBackup& backup)
{
    if (!managing_)
    {
        return FileResult::TheStartupEntriesAreLeftLoose;
    }

    if (processProbe_.SimulatorIsRunning())
    {
        return FileResult::TheSimulatorIsRunning;
    }

    return entries_.Switch(entryPath, enabled, backup);
}

StartupApplied StartupService::Apply(const StartupChange& change, StartupBackup& backup)
{
    if (!managing_)
    {
        return StartupApplied{.result = FileResult::TheStartupEntriesAreLeftLoose};
    }

    const bool touchesTheStartupFile = !std::holds_alternative<StartupForgetting>(change);

    if (touchesTheStartupFile && processProbe_.SimulatorIsRunning())
    {
        return StartupApplied{.result = FileResult::TheSimulatorIsRunning};
    }

    return entries_.Apply(change, backup);
}

std::vector<StartupRemovedEntry> StartupService::Removed() const
{
    if (!managing_)
    {
        return {};
    }

    return entries_.Removed();
}
