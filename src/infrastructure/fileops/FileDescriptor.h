#ifndef FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_FILE_DESCRIPTOR_H
#define FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_FILE_DESCRIPTOR_H

#include <unistd.h>

class FileDescriptor
{
public:
    explicit FileDescriptor(const int value) : value_(value)
    {
    }

    FileDescriptor(const FileDescriptor&) = delete;

    FileDescriptor& operator=(const FileDescriptor&) = delete;

    ~FileDescriptor()
    {
        if (value_ >= 0)
        {
            ::close(value_);
        }
    }

    [[nodiscard]] int Value() const
    {
        return value_;
    }

    [[nodiscard]] bool IsOpen() const
    {
        return value_ >= 0;
    }

private:
    int value_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_FILE_DESCRIPTOR_H
