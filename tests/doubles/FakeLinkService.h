#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_LINK_SERVICE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_LINK_SERVICE_H

#include <algorithm>
#include <string>
#include <vector>

#include "domain/ports/LinkService.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/InMemoryFileSystem.h"

class FakeLinkService final : public LinkService
{
public:
    explicit FakeLinkService(InMemoryFileSystem& fileSystem) : fileSystem_(fileSystem)
    {
    }

    void MakeLinkCreationFail()
    {
        MakeLinkCreationFailWith(LinkFailure::CouldNotCreateLink);
    }

    void MakeLinkCreationFailWith(const LinkFailure refusal)
    {
        linkCreationRefusal_ = refusal;
    }

    void MakeLinkRemovalFail()
    {
        linkRemovalFails_ = true;
    }

    void MakeTheRemovalFailFor(const std::filesystem::path& linkPath)
    {
        unremovable_.push_back(ComparablePath(linkPath));
    }

    [[nodiscard]] LinkFailure CreateLink(const std::filesystem::path& linkPath,
                                         const std::filesystem::path& target,
                                         const LinkType linkType) override
    {
        lastLinkType = linkType;

        if (linkCreationRefusal_ != LinkFailure::None)
        {
            return linkCreationRefusal_;
        }
        if (fileSystem_.Exists(linkPath))
        {
            return LinkFailure::CouldNotCreateLink;
        }
        fileSystem_.AddLink(linkPath, target);
        return LinkFailure::None;
    }

    [[nodiscard]] bool RemoveReparseNode(const std::filesystem::path& linkPath) override
    {
        if (linkRemovalFails_ || std::ranges::find(unremovable_, ComparablePath(linkPath)) != unremovable_.end())
        {
            return false;
        }

        return fileSystem_.RemoveNode(linkPath);
    }

    [[nodiscard]] std::optional<std::filesystem::path> ReadLinkTarget(const std::filesystem::path& path) const override
    {
        return fileSystem_.LinkTarget(path);
    }

    LinkType lastLinkType = LinkType::Junction;

private:
    InMemoryFileSystem& fileSystem_;
    std::vector<std::string> unremovable_;
    LinkFailure linkCreationRefusal_ = LinkFailure::None;
    bool linkRemovalFails_ = false;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_LINK_SERVICE_H
