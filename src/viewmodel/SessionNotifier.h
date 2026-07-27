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

signals:
    void ScanStarted();

    void ScanFinished();

    void Refreshed();
};

#endif // FS_ORGANIZER_VIEWMODEL_SESSION_NOTIFIER_H
