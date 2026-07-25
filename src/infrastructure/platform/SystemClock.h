#ifndef FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_SYSTEM_CLOCK_H
#define FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_SYSTEM_CLOCK_H

#include "domain/ports/Clock.h"

class SystemClock final : public Clock
{
public:
    [[nodiscard]] std::chrono::system_clock::time_point Now() const override
    {
        return std::chrono::system_clock::now();
    }
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_SYSTEM_CLOCK_H
