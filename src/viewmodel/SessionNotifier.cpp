#include "viewmodel/SessionNotifier.h"

SessionNotifier::SessionNotifier(QObject* parent) : QObject(parent)
{
}

void SessionNotifier::OnScanStarted()
{
    emit ScanStarted();
}

void SessionNotifier::OnScanFinished()
{
    emit ScanFinished();
}

void SessionNotifier::OnRefreshed()
{
    emit Refreshed();
}
