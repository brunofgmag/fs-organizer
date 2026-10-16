#ifndef FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_LINUX_INSTANCE_GUARD_H
#define FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_LINUX_INSTANCE_GUARD_H

#include <filesystem>
#include <memory>

#include <QtCore/QLockFile>
#include <QtCore/QString>
#include <QtNetwork/QLocalServer>

#include "infrastructure/platform/InstanceGuard.h"

class LinuxInstanceGuard final : public InstanceGuard
{
public:
    explicit LinuxInstanceGuard(const std::filesystem::path& lockFile);

    [[nodiscard]] bool AnotherIsRunning() const override;

    void BringTheRunningOneForward() const override;

private:
    void AnswerTheNextLaunch();

    QLockFile lock_;
    QString socketName_;
    bool anotherIsRunning_ = false;
    std::unique_ptr<QLocalServer> server_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_LINUX_INSTANCE_GUARD_H
