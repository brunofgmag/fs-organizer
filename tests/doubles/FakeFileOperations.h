#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILE_OPERATIONS_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILE_OPERATIONS_H

#include "domain/ports/FileOperations.h"
#include "tests/doubles/InMemoryFileSystem.h"

class FakeFileOperations final : public FileOperations
{
public:
    explicit FakeFileOperations(InMemoryFileSystem& fileSystem)
        : fileSystem_(fileSystem)
    {
    }

    [[nodiscard]] bool DirectoryExists(const std::filesystem::path& path) const override
    {
        return fileSystem_.Exists(path);
    }

private:
    InMemoryFileSystem& fileSystem_;
};

#endif
