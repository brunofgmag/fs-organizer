#include "infrastructure/platform/LinuxInstanceGuard.h"

bool LinuxInstanceGuard::AnotherIsRunning() const
{
    return false;
}

void LinuxInstanceGuard::BringTheRunningOneForward() const
{
}
