#include "infrastructure/platform/WindowsInstanceGuard.h"

#include <QtCore/QCoreApplication>

WindowsInstanceGuard::WindowsInstanceGuard(const std::wstring& name) : single_(name)
{
}

bool WindowsInstanceGuard::AnotherIsRunning() const
{
    return single_.AnotherIsRunning();
}

void WindowsInstanceGuard::BringTheRunningOneForward() const
{
    BringTheRunningInstanceForward(QCoreApplication::applicationName().toStdWString());
}
