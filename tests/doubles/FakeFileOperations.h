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

    [[nodiscard]] bool EntryExistsWithoutFollowingLinks(
            const std::filesystem::path& path) const override
    {
        return fileSystem_.Exists(path);
    }

    [[nodiscard]] bool TargetDirectoryExists(const std::filesystem::path& path) const override
    {
        return fileSystem_.IsDirectory(path);
    }

    [[nodiscard]] std::vector<std::filesystem::path> ChildDirectories(
        const std::filesystem::path& path) const override
    {
        return fileSystem_.ChildrenOf(path);
    }

    [[nodiscard]] bool VolumeIsAvailable(const std::filesystem::path& path) const override
    {
        return fileSystem_.VolumeIsAvailable(path);
    }

private:
    InMemoryFileSystem& fileSystem_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILE_OPERATIONS_H
