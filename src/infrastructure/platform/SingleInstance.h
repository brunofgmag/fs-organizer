#ifndef FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_SINGLE_INSTANCE_H
#define FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_SINGLE_INSTANCE_H

#include <string>

class SingleInstance
{
public:
    explicit SingleInstance(const std::wstring& name);

    SingleInstance(const SingleInstance&) = delete;

    SingleInstance& operator=(const SingleInstance&) = delete;

    ~SingleInstance();

    [[nodiscard]] bool AnotherIsRunning() const;

private:
    void* handle_ = nullptr;
    bool anotherIsRunning_ = false;
};

void BringTheRunningInstanceForward(const std::wstring& windowTitle);

#endif // FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_SINGLE_INSTANCE_H
