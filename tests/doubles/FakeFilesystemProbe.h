#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILESYSTEM_PROBE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILESYSTEM_PROBE_H

#include <algorithm>

#include "domain/ports/FilesystemProbe.h"
#include "tests/doubles/InMemoryFileSystem.h"

class FakeFilesystemProbe final : public FilesystemProbe
{
public:
    explicit FakeFilesystemProbe(InMemoryFileSystem& fileSystem) : fileSystem_(fileSystem)
    {
    }

    [[nodiscard]] bool EntryExistsWithoutFollowingLinks(const std::filesystem::path& path) const override
    {
        return fileSystem_.Exists(path);
    }

    [[nodiscard]] bool IsReparsePoint(const std::filesystem::path& path) const override
    {
        return fileSystem_.IsLink(path);
    }

    [[nodiscard]] bool TargetDirectoryExists(const std::filesystem::path& path) const override
    {
        return fileSystem_.IsDirectory(path);
    }

    [[nodiscard]] std::vector<std::filesystem::path> ChildDirectories(const std::filesystem::path& path) const override
    {
        enumerated.push_back(path);

        return fileSystem_.ChildDirectoriesOf(path);
    }

    [[nodiscard]] bool WasEnumerated(const std::filesystem::path& path) const
    {
        return std::ranges::find(enumerated, path) != enumerated.end();
    }

    mutable std::vector<std::filesystem::path> enumerated;

    [[nodiscard]] bool VolumeIsAvailable(const std::filesystem::path& path) const override
    {
        return fileSystem_.VolumeIsAvailable(path);
    }

    [[nodiscard]] bool ProbeWritable(const std::filesystem::path& path) const override
    {
        return fileSystem_.IsWritable(path);
    }

    [[nodiscard]] std::optional<std::uintmax_t> FreeSpaceOn(const std::filesystem::path& path) const override
    {
        return fileSystem_.FreeSpaceOn(path);
    }

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point>
    LastWriteTime(const std::filesystem::path& path) const override
    {
        return fileSystem_.LastWriteTime(path);
    }

    [[nodiscard]] std::vector<FileFingerprint> FingerprintTree(const std::filesystem::path& root) const override
    {
        std::vector<FileFingerprint> files;
        for (const std::filesystem::path& file : fileSystem_.FilesUnder(root))
        {
            files.push_back(FileFingerprint{file.lexically_relative(root), fileSystem_.FileSize(file)});
        }
        return files;
    }

private:
    InMemoryFileSystem& fileSystem_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILESYSTEM_PROBE_H
