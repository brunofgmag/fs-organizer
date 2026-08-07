#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILE_OPERATIONS_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILE_OPERATIONS_H

#include <algorithm>
#include <string>

#include "domain/ports/FileOperations.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/InMemoryFileSystem.h"

class FakeFileOperations final : public FileOperations
{
public:
    explicit FakeFileOperations(InMemoryFileSystem& fileSystem) : fileSystem_(fileSystem)
    {
    }

    void MakeTheCopyFailPartWayThrough()
    {
        copyFailsPartWayThrough_ = true;
    }

    void MakeTheCopyDropAFile()
    {
        copyDropsAFile_ = true;
    }

    void MakeTheMoveFail()
    {
        moveFails_ = true;
    }

    void MakeTheRemovalFail()
    {
        removalFails_ = true;
    }

    void MakeTheRemovalFailFor(const std::filesystem::path& path)
    {
        unremovable_.push_back(ComparablePath(path));
    }

    void MakeTheCreationFail()
    {
        creationFails_ = true;
    }

    void MakeTheHiddenWriteFail()
    {
        theHiddenWriteFails_ = true;
    }

    [[nodiscard]] CopyOutcome CopyTree(const std::filesystem::path& source,
                                       const std::filesystem::path& destination,
                                       const std::function<bool(const CopyProgress&)>& onProgress) override
    {
        const std::vector<std::filesystem::path> files = fileSystem_.FilesUnder(source);

        std::uintmax_t total = 0;
        for (const std::filesystem::path& file : files)
        {
            total += fileSystem_.FileSize(file);
        }

        fileSystem_.AddDirectory(destination);

        std::uintmax_t copied = 0;
        for (const std::filesystem::path& file : files)
        {
            if (onProgress && !onProgress(CopyProgress{.copiedBytes = copied, .totalBytes = total}))
            {
                return CopyOutcome::Cancelled;
            }

            const std::filesystem::path landing = destination / file.lexically_relative(source);
            for (std::filesystem::path folder = landing.parent_path();
                 folder != destination && folder.has_relative_path(); folder = folder.parent_path())
            {
                fileSystem_.AddDirectory(folder);
            }

            const std::uintmax_t size = fileSystem_.FileSize(file);
            if (!copyDropsAFile_ || file != files.back())
            {
                fileSystem_.AddFile(landing, size);
            }

            copied += size;

            if (copyFailsPartWayThrough_)
            {
                return CopyOutcome::Failed;
            }
        }

        return CopyOutcome::Completed;
    }

    [[nodiscard]] bool CreateFolder(const std::filesystem::path& path) override
    {
        if (creationFails_ || fileSystem_.Exists(path))
        {
            return false;
        }

        fileSystem_.AddDirectory(path);

        return true;
    }

    [[nodiscard]] bool WriteHiddenFile(const std::filesystem::path& path) override
    {
        if (theHiddenWriteFails_)
        {
            return false;
        }

        fileSystem_.AddFile(path);

        return true;
    }

    [[nodiscard]] bool Move(const std::filesystem::path& source, const std::filesystem::path& destination) override
    {
        return !moveFails_ && fileSystem_.MoveTree(source, destination);
    }

    [[nodiscard]] bool RemoveTree(const std::filesystem::path& path) override
    {
        return TheRemovalIsAllowed(path) && fileSystem_.RemoveTree(path);
    }

    [[nodiscard]] bool Recycle(const std::filesystem::path& path) override
    {
        return TheRemovalIsAllowed(path) && fileSystem_.RecycleTree(path);
    }

    [[nodiscard]] bool RemoveEmptyFolder(const std::filesystem::path& path) override
    {
        return !removalFails_ && fileSystem_.RemoveEmptyDirectory(path);
    }

private:
    [[nodiscard]] bool TheRemovalIsAllowed(const std::filesystem::path& path) const
    {
        return !removalFails_ && std::ranges::find(unremovable_, ComparablePath(path)) == unremovable_.end();
    }

    InMemoryFileSystem& fileSystem_;
    std::vector<std::string> unremovable_;
    bool copyFailsPartWayThrough_ = false;
    bool copyDropsAFile_ = false;
    bool moveFails_ = false;
    bool removalFails_ = false;
    bool creationFails_ = false;
    bool theHiddenWriteFails_ = false;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_FILE_OPERATIONS_H
