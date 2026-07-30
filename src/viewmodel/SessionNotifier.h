#ifndef FS_ORGANIZER_VIEWMODEL_SESSION_NOTIFIER_H
#define FS_ORGANIZER_VIEWMODEL_SESSION_NOTIFIER_H

#include <QtCore/QObject>

#include "application/ports/SessionObserver.h"

class SessionNotifier final : public QObject, public SessionObserver
{
    Q_OBJECT

public:
    explicit SessionNotifier(QObject* parent = nullptr);

    void OnScanStarted() override;

    void OnScanFinished() override;

    void OnRefreshed() override;

    void OnSettingsCouldNotBeSaved() override;

    void OnSimulatorIsRunning() override;

    void OnRestartPendingChanged(bool pending) override;

signals:
    void ScanStarted();

    void ScanFinished();

    void Refreshed();

    void SettingsCouldNotBeSaved();

    void SimulatorIsRunning();

    void RestartPendingChanged(bool pending);
};

#endif // FS_ORGANIZER_VIEWMODEL_SESSION_NOTIFIER_H
