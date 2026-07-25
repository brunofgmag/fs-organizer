#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_LINK_SERVICE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_LINK_SERVICE_H

#include "domain/ports/LinkService.h"
#include "tests/doubles/InMemoryFileSystem.h"

class FakeLinkService final : public LinkService
{
public:
    explicit FakeLinkService(InMemoryFileSystem& fileSystem)
        : fileSystem_(fileSystem)
    {
    }

    [[nodiscard]] bool CreateLink(const std::filesystem::path& linkPath,
                                  const std::filesystem::path& target,
                                  LinkType) override
    {
        if (fileSystem_.Exists(linkPath))
        {
            return false;
        }
        fileSystem_.AddLink(linkPath, target);
        return true;
    }

    [[nodiscard]] bool RemoveReparseNode(const std::filesystem::path& linkPath) override
    {
        return fileSystem_.RemoveNode(linkPath);
    }

    [[nodiscard]] bool IsReparsePoint(const std::filesystem::path& path) const override
    {
        return fileSystem_.IsLink(path);
    }

    [[nodiscard]] std::optional<std::filesystem::path>
    ReadLinkTarget(const std::filesystem::path& path) const override
    {
        return fileSystem_.LinkTarget(path);
    }

private:
    InMemoryFileSystem& fileSystem_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_LINK_SERVICE_H
