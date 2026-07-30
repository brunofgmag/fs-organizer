#ifndef FS_ORGANIZER_TESTS_DOUBLES_RECORDING_SESSION_OBSERVER_H
#define FS_ORGANIZER_TESTS_DOUBLES_RECORDING_SESSION_OBSERVER_H

#include "application/ports/SessionObserver.h"

class RecordingSessionObserver final : public SessionObserver
{
public:
    void OnScanStarted() override
    {
        ++started;
    }

    void OnScanFinished() override
    {
        ++finished;
    }

    void OnRefreshed() override
    {
        ++refreshed;
    }

    void OnSettingsCouldNotBeSaved() override
    {
        ++settingsRefused;
    }

    void OnSimulatorIsRunning() override
    {
        ++simulatorWarnings;
    }

    void OnRestartPendingChanged(const bool pending) override
    {
        restartPending = pending;
        ++restartReports;
    }

    int started = 0;
    int finished = 0;
    int refreshed = 0;
    int settingsRefused = 0;
    int simulatorWarnings = 0;
    int restartReports = 0;
    bool restartPending = false;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_RECORDING_SESSION_OBSERVER_H
