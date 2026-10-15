#ifndef FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_WINDOWS_INSTANCE_GUARD_H
#define FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_WINDOWS_INSTANCE_GUARD_H

#include <string>

#include "infrastructure/platform/InstanceGuard.h"
#include "infrastructure/platform/SingleInstance.h"

class WindowsInstanceGuard final : public InstanceGuard
{
public:
    explicit WindowsInstanceGuard(const std::wstring& name);

    [[nodiscard]] bool AnotherIsRunning() const override;

    void BringTheRunningOneForward() const override;

private:
    SingleInstance single_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_WINDOWS_INSTANCE_GUARD_H
