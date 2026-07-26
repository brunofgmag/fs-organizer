#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_LINK_SERVICE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_LINK_SERVICE_H

#include "domain/ports/LinkService.h"
#include "tests/doubles/InMemoryFileSystem.h"

class FakeLinkService final : public LinkService
{
public:
    explicit FakeLinkService(InMemoryFileSystem& fileSystem) : fileSystem_(fileSystem)
    {
    }

    void MakeLinkCreationFail()
    {
        linkCreationFails_ = true;
    }

    void MakeLinkRemovalFail()
    {
        linkRemovalFails_ = true;
    }

    [[nodiscard]] bool
    CreateLink(const std::filesystem::path& linkPath, const std::filesystem::path& target, LinkType) override
    {
        if (linkCreationFails_ || fileSystem_.Exists(linkPath))
        {
            return false;
        }
        fileSystem_.AddLink(linkPath, target);
        return true;
    }

    [[nodiscard]] bool RemoveReparseNode(const std::filesystem::path& linkPath) override
    {
        return !linkRemovalFails_ && fileSystem_.RemoveNode(linkPath);
    }

    [[nodiscard]] std::optional<std::filesystem::path> ReadLinkTarget(const std::filesystem::path& path) const override
    {
        return fileSystem_.LinkTarget(path);
    }

private:
    InMemoryFileSystem& fileSystem_;
    bool linkCreationFails_ = false;
    bool linkRemovalFails_ = false;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_LINK_SERVICE_H
