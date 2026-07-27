#ifndef FS_ORGANIZER_TESTS_DOUBLES_INLINE_BACKGROUND_RUNNER_H
#define FS_ORGANIZER_TESTS_DOUBLES_INLINE_BACKGROUND_RUNNER_H

#include <functional>
#include <utility>

#include "application/ports/BackgroundRunner.h"

class InlineBackgroundRunner final : public BackgroundRunner
{
public:
    void Run(std::function<void()> work, std::function<void()> done) override
    {
        ++runs;

        if (!defer)
        {
            work();
            done();
            return;
        }

        work_ = std::move(work);
        done_ = std::move(done);
    }

    void Finish()
    {
        const std::function<void()> work = std::exchange(work_, {});
        const std::function<void()> done = std::exchange(done_, {});

        if (work)
        {
            work();
        }

        if (done)
        {
            done();
        }
    }

    [[nodiscard]] bool Pending() const
    {
        return static_cast<bool>(done_);
    }

    bool defer = false;
    int runs = 0;

private:
    std::function<void()> work_;
    std::function<void()> done_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_INLINE_BACKGROUND_RUNNER_H
