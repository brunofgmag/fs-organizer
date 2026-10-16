#ifndef FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_INSTANCE_GUARD_H
#define FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_INSTANCE_GUARD_H

class InstanceGuard
{
public:
    virtual ~InstanceGuard() = default;

    [[nodiscard]] virtual bool AnotherIsRunning() const = 0;

    virtual void BringTheRunningOneForward() const = 0;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_INSTANCE_GUARD_H
