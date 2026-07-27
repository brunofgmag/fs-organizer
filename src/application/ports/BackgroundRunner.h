#ifndef FS_ORGANIZER_APPLICATION_PORTS_BACKGROUND_RUNNER_H
#define FS_ORGANIZER_APPLICATION_PORTS_BACKGROUND_RUNNER_H

#include <functional>

class BackgroundRunner
{
public:
    virtual ~BackgroundRunner() = default;

    virtual void Run(std::function<void()> work, std::function<void()> doneOnTheCallingThread) = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_BACKGROUND_RUNNER_H
