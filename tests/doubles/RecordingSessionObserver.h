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

    int started = 0;
    int finished = 0;
    int refreshed = 0;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_RECORDING_SESSION_OBSERVER_H
