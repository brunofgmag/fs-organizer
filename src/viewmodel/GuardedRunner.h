#ifndef FS_ORGANIZER_VIEWMODEL_GUARDED_RUNNER_H
#define FS_ORGANIZER_VIEWMODEL_GUARDED_RUNNER_H

#include <functional>

#include "application/ports/BackgroundRunner.h"

class GuardedRunner
{
public:
    explicit GuardedRunner(BackgroundRunner& runner);

    [[nodiscard]] bool Busy() const;

    void Run(std::function<void()> work, std::function<void()> done);

    void Run(const std::function<void()>& starting, std::function<void()> work, std::function<void()> done);

private:
    BackgroundRunner& runner_;
    bool busy_ = false;
};

#endif // FS_ORGANIZER_VIEWMODEL_GUARDED_RUNNER_H
