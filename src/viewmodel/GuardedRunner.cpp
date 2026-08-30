#include "viewmodel/GuardedRunner.h"

#include <utility>

GuardedRunner::GuardedRunner(BackgroundRunner& runner) : runner_(runner)
{
}

bool GuardedRunner::Busy() const
{
    return busy_;
}

void GuardedRunner::Run(std::function<void()> work, std::function<void()> done)
{
    Run({}, std::move(work), std::move(done));
}

void GuardedRunner::Run(const std::function<void()>& starting, std::function<void()> work, std::function<void()> done)
{
    if (busy_)
    {
        return;
    }

    busy_ = true;

    if (starting)
    {
        starting();
    }

    runner_.Run(std::move(work),
                [this, done = std::move(done)]
                {
                    busy_ = false;

                    done();
                });
}
