#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_SIDECAR_STORE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_SIDECAR_STORE_H

#include <filesystem>
#include <optional>
#include <string>

#include "domain/ports/SidecarStore.h"
#include "tests/doubles/InMemoryFileSystem.h"

class FakeSidecarStore final : public SidecarStore
{
public:
    explicit FakeSidecarStore(InMemoryFileSystem& fileSystem) : fileSystem_(fileSystem)
    {
    }

    void MakeTheWriteFail()
    {
        theWriteFails_ = true;
    }

    [[nodiscard]] bool Write(const std::filesystem::path& path, const std::string& contents) override
    {
        if (theWriteFails_ || !fileSystem_.IsDirectory(path.parent_path()))
        {
            return false;
        }

        fileSystem_.AddFileWithContents(path, contents);

        return true;
    }

    [[nodiscard]] std::optional<std::string> Read(const std::filesystem::path& path) const override
    {
        return fileSystem_.ContentsOf(path);
    }

    [[nodiscard]] bool Forget(const std::filesystem::path& path) override
    {
        return !fileSystem_.IsDirectory(path) && fileSystem_.RemoveTree(path);
    }

private:
    InMemoryFileSystem& fileSystem_;
    bool theWriteFails_ = false;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_SIDECAR_STORE_H
