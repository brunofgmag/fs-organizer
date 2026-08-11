#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILESYSTEM_PROBE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILESYSTEM_PROBE_H

#include <algorithm>
#include <string_view>

#include "domain/ports/FilesystemProbe.h"
#include "domain/support/PathUtils.h"
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
        if (fileSystem_.IsDirectory(path))
        {
            return true;
        }

        const std::optional<std::filesystem::path> target = fileSystem_.LinkTarget(path);

        return target.has_value() && fileSystem_.IsDirectory(*target);
    }

    [[nodiscard]] bool PhysicalDirectoryExists(const std::filesystem::path& path) const override
    {
        lookedAt.push_back(ComparablePath(path));

        return fileSystem_.IsDirectory(path) && !fileSystem_.IsLink(path);
    }

    [[nodiscard]] std::size_t TimesLookedAt(const std::filesystem::path& path) const
    {
        return static_cast<std::size_t>(std::ranges::count(lookedAt, ComparablePath(path)));
    }

    mutable std::vector<std::string> lookedAt;

    [[nodiscard]] std::vector<std::filesystem::path> ChildDirectories(const std::filesystem::path& path) const override
    {
        enumerated.push_back(path);

        return fileSystem_.ChildDirectoriesOf(path);
    }

    [[nodiscard]] std::vector<std::filesystem::path> ChildFiles(const std::filesystem::path& path) const override
    {
        enumerated.push_back(path);

        return fileSystem_.ChildFilesOf(path);
    }

    [[nodiscard]] bool WasEnumerated(const std::filesystem::path& path) const
    {
        return std::ranges::find(enumerated, path) != enumerated.end();
    }

    [[nodiscard]] std::size_t TimesEnumerated(const std::filesystem::path& path) const
    {
        return static_cast<std::size_t>(std::ranges::count(enumerated, path));
    }

    mutable std::vector<std::filesystem::path> enumerated;

    [[nodiscard]] bool VolumeIsAvailable(const std::filesystem::path& path) const override
    {
        return fileSystem_.VolumeIsAvailable(path);
    }

    [[nodiscard]] WriteAccess ProbeWritable(const std::filesystem::path& path) const override
    {
        return fileSystem_.WriteAccessOn(path);
    }

    [[nodiscard]] std::optional<std::uintmax_t> FreeSpaceOn(const std::filesystem::path& path) const override
    {
        return fileSystem_.FreeSpaceOn(path);
    }

    [[nodiscard]] std::size_t TimesTheRecycleBinWasAsked() const
    {
        return recycleBinAsked;
    }

    [[nodiscard]] std::optional<RecycleBinRoom> RecycleBinOn(const std::filesystem::path& path) const override
    {
        ++recycleBinAsked;

        const std::optional<std::uintmax_t> quota = fileSystem_.RecycleBinQuotaOn(path);
        if (!quota.has_value())
        {
            return std::nullopt;
        }

        return RecycleBinRoom{.quota = *quota, .itRecycles = fileSystem_.VolumeRecycles(path)};
    }

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point>
    LastWriteTime(const std::filesystem::path& path) const override
    {
        return fileSystem_.LastWriteTime(path);
    }

    [[nodiscard]] std::optional<std::string> ContentsOf(const std::filesystem::path& path) const override
    {
        read.push_back(ComparablePath(path));

        return fileSystem_.ContentsOf(path);
    }

    [[nodiscard]] std::optional<std::string> FirstBytesOf(const std::filesystem::path& path,
                                                          const std::size_t most) const override
    {
        read.push_back(ComparablePath(path));

        std::optional<std::string> contents = fileSystem_.ContentsOf(path);
        if (contents.has_value() && contents->size() > most)
        {
            contents->resize(most);
        }

        return contents;
    }

    [[nodiscard]] std::size_t TimesItReadSomethingEndingIn(const std::string_view suffix) const
    {
        return static_cast<std::size_t>(std::ranges::count_if(read,
                                                              [suffix](const std::string& path)
                                                              {
                                                                  return path.ends_with(suffix);
                                                              }));
    }

    mutable std::vector<std::string> read;

    [[nodiscard]] std::optional<TreeFingerprint> FingerprintTree(const std::filesystem::path& root) const override
    {
        walked.push_back(root);

        if (std::ranges::find(unreadable_, ComparablePath(root)) != unreadable_.end())
        {
            return std::nullopt;
        }

        const std::optional<std::size_t> longest = fileSystem_.LongestEntryUnder(root);
        if (!longest.has_value())
        {
            return std::nullopt;
        }

        TreeFingerprint walk{.longestEntry = *longest};
        for (const std::filesystem::path& file : fileSystem_.FilesUnder(root))
        {
            walk.files.push_back(
                FileFingerprint{.relativePath = file.lexically_relative(root), .size = fileSystem_.FileSize(file)});
        }

        return walk;
    }

    void RefuseToWalk(const std::filesystem::path& root)
    {
        unreadable_.push_back(ComparablePath(root));
    }

    [[nodiscard]] std::size_t TimesWalked(const std::filesystem::path& root) const
    {
        return static_cast<std::size_t>(std::ranges::count(walked, root));
    }

    mutable std::vector<std::filesystem::path> walked;

private:
    InMemoryFileSystem& fileSystem_;
    std::vector<std::string> unreadable_;
    mutable std::size_t recycleBinAsked = 0;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILESYSTEM_PROBE_H
