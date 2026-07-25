#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILESYSTEM_PROBE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILESYSTEM_PROBE_H

#include "domain/ports/FilesystemProbe.h"
#include "tests/doubles/InMemoryFileSystem.h"

class FakeFilesystemProbe final : public FilesystemProbe
{
public:
    explicit FakeFilesystemProbe(InMemoryFileSystem& fileSystem)
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
        return fileSystem_.ChildDirectoriesOf(path);
    }

    [[nodiscard]] bool VolumeIsAvailable(const std::filesystem::path& path) const override
    {
        return fileSystem_.VolumeIsAvailable(path);
    }

    [[nodiscard]] bool ProbeWritable(const std::filesystem::path& path) const override
    {
        return fileSystem_.IsWritable(path);
    }

private:
    InMemoryFileSystem& fileSystem_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILESYSTEM_PROBE_H
