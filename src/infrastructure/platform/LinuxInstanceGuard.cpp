#include "infrastructure/platform/LinuxInstanceGuard.h"

#include <system_error>

#include <QtCore/QtLogging>

#include "support/PathText.h"

LinuxInstanceGuard::LinuxInstanceGuard(const std::filesystem::path& lockFile) : lock_(AsText(lockFile))
{
    std::error_code error;
    std::filesystem::create_directories(lockFile.parent_path(), error);

    lock_.setStaleLockTime(0);
    anotherIsRunning_ = !lock_.tryLock(0) && lock_.error() == QLockFile::LockFailedError;
}

bool LinuxInstanceGuard::AnotherIsRunning() const
{
    return anotherIsRunning_;
}

void LinuxInstanceGuard::BringTheRunningOneForward() const
{
    qWarning("FS Organizer is already running, so this launch exits. Raise the open window yourself.");
}
