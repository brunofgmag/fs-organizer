#ifndef FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_LINUX_INSTANCE_GUARD_H
#define FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_LINUX_INSTANCE_GUARD_H

#include "infrastructure/platform/InstanceGuard.h"

class LinuxInstanceGuard final : public InstanceGuard
{
public:
    [[nodiscard]] bool AnotherIsRunning() const override;

    void BringTheRunningOneForward() const override;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_LINUX_INSTANCE_GUARD_H
