#ifndef FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_LINUX_INSTANCE_GUARD_H
#define FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_LINUX_INSTANCE_GUARD_H

#include <filesystem>

#include <QtCore/QLockFile>

#include "infrastructure/platform/InstanceGuard.h"

class LinuxInstanceGuard final : public InstanceGuard
{
public:
    explicit LinuxInstanceGuard(const std::filesystem::path& lockFile);

    [[nodiscard]] bool AnotherIsRunning() const override;

    void BringTheRunningOneForward() const override;

private:
    QLockFile lock_;
    bool anotherIsRunning_ = false;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_LINUX_INSTANCE_GUARD_H
