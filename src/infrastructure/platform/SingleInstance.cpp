#include "infrastructure/platform/SingleInstance.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

SingleInstance::SingleInstance(const std::wstring& name)
{
    handle_ = CreateMutexW(nullptr, TRUE, name.c_str());
    anotherIsRunning_ = handle_ != nullptr && GetLastError() == ERROR_ALREADY_EXISTS;
}

SingleInstance::~SingleInstance()
{
    if (handle_ == nullptr)
    {
        return;
    }

    if (!anotherIsRunning_)
    {
        ReleaseMutex(handle_);
    }

    CloseHandle(handle_);
}

bool SingleInstance::AnotherIsRunning() const
{
    return anotherIsRunning_;
}

void BringTheRunningInstanceForward(const std::wstring& windowTitle)
{
    const HWND window = FindWindowW(nullptr, windowTitle.c_str());

    if (window == nullptr)
    {
        return;
    }

    if (IsIconic(window))
    {
        ShowWindow(window, SW_RESTORE);
    }

    SetForegroundWindow(window);
}
