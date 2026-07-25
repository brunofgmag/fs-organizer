#ifndef FS_ORGANIZER_APPLICATION_PORTS_CLOCK_H
#define FS_ORGANIZER_APPLICATION_PORTS_CLOCK_H

#include <chrono>

class Clock
{
public:
    virtual ~Clock() = default;

    [[nodiscard]] virtual std::chrono::system_clock::time_point Now() const = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_CLOCK_H
