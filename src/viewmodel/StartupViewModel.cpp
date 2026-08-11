#include "viewmodel/StartupViewModel.h"

StartupViewModel::StartupViewModel(StartupService& service, Session& session, const Clock& clock, QObject* parent)
    : QObject(parent), service_(service), session_(session), clock_(clock)
{
}

void StartupViewModel::Show()
{
    Read();

    emit Changed();
}

bool StartupViewModel::Managing() const
{
    return service_.Managing();
}

void StartupViewModel::Manage(const bool managing)
{
    if (service_.Managing() == managing)
    {
        return;
    }

    const bool written = session_.Rewrite(
        [managing](AppSettings& settings)
        {
            settings.manageStartupEntries = managing;

            return true;
        });

    if (!written)
    {
        emit SettingsCouldNotBeSaved();
        return;
    }

    service_.Manage(managing);
    Read();

    emit Changed();
}

const std::vector<StartupLine>& StartupViewModel::Lines() const
{
    return report_.lines;
}

std::optional<std::chrono::system_clock::time_point> StartupViewModel::ReadAt() const
{
    return readAt_;
}

std::optional<std::string> StartupViewModel::RunningSimulator() const
{
    return service_.RunningSimulator();
}

FileResult StartupViewModel::Switch(const std::filesystem::path& entryPath, const bool enabled)
{
    const FileResult result = service_.Switch(entryPath, enabled);
    if (!Succeeded(result))
    {
        return result;
    }

    Read();

    emit Changed();

    return result;
}

void StartupViewModel::Read()
{
    if (!service_.Managing())
    {
        report_ = {};
        readAt_.reset();

        return;
    }

    report_ = service_.Report(session_.Profile(), session_.Snapshot());
    readAt_ = clock_.Now();
}
