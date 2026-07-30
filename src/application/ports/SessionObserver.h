#ifndef FS_ORGANIZER_APPLICATION_PORTS_SESSION_OBSERVER_H
#define FS_ORGANIZER_APPLICATION_PORTS_SESSION_OBSERVER_H

class SessionObserver
{
public:
    virtual ~SessionObserver() = default;

    virtual void OnScanStarted() = 0;

    virtual void OnScanFinished() = 0;

    virtual void OnRefreshed() = 0;

    virtual void OnSettingsCouldNotBeSaved() = 0;

    virtual void OnSimulatorIsRunning() = 0;

    virtual void OnRestartPendingChanged(bool pending) = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_SESSION_OBSERVER_H
