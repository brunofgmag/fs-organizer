#ifndef FS_ORGANIZER_TESTS_DOUBLES_INLINE_BACKGROUND_RUNNER_H
#define FS_ORGANIZER_TESTS_DOUBLES_INLINE_BACKGROUND_RUNNER_H

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

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

        deferred_.push_back(Deferred{.work = std::move(work), .done = std::move(done)});
    }

    void Finish()
    {
        if (deferred_.empty())
        {
            return;
        }

        Deferred oldest = std::move(deferred_.front());
        deferred_.erase(deferred_.begin());
        Complete(oldest);
    }

    void RunPendingWork()
    {
        for (Deferred& entry : deferred_)
        {
            if (!entry.worked && entry.work)
            {
                entry.work();
                entry.worked = true;
            }
        }
    }

    void FinishNewestDone()
    {
        if (deferred_.empty())
        {
            return;
        }

        Deferred newest = std::move(deferred_.back());
        deferred_.pop_back();
        Complete(newest);
    }

    [[nodiscard]] bool Pending() const
    {
        return !deferred_.empty();
    }

    [[nodiscard]] std::size_t HowManyPending() const
    {
        return deferred_.size();
    }

    bool defer = false;
    int runs = 0;

private:
    struct Deferred
    {
        std::function<void()> work;
        std::function<void()> done;
        bool worked = false;
    };

    static void Complete(const Deferred& entry)
    {
        if (!entry.worked && entry.work)
        {
            entry.work();
        }

        if (entry.done)
        {
            entry.done();
        }
    }

    std::vector<Deferred> deferred_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_INLINE_BACKGROUND_RUNNER_H
