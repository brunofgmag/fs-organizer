#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_CLOCK_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_CLOCK_H

#include "domain/ports/Clock.h"

class FakeClock final : public Clock
{
public:
    [[nodiscard]] std::chrono::system_clock::time_point Now() const override
    {
        return now;
    }

    std::chrono::system_clock::time_point now{std::chrono::seconds{1'769'000'000}};
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_CLOCK_H
