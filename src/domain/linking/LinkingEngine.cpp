#include "domain/linking/LinkingEngine.h"

#include "domain/support/PathUtils.h"

LinkingEngine::LinkingEngine(LinkService& linkService, const FilesystemProbe& filesystemProbe)
    : linkService_(linkService), filesystemProbe_(filesystemProbe)
{
}

LinkOutcome
LinkingEngine::Enable(const Addon& addon, const std::filesystem::path& destinationRoot, const LinkType linkType) const
{
    const std::filesystem::path linkPath = destinationRoot / addon.folderPath.filename();

    if (filesystemProbe_.EntryExistsWithoutFollowingLinks(linkPath))
    {
        if (!filesystemProbe_.IsReparsePoint(linkPath))
        {
            return LinkOutcome::Conflicted(CopyConflict{linkPath, addon.folderPath});
        }

        const std::optional<std::filesystem::path> target = linkService_.ReadLinkTarget(linkPath);
        if (!target.has_value())
        {
            return LinkOutcome::Failed(LinkFailure::UnreadableLinkTarget);
        }

        const std::filesystem::path pointsAt = NormalizeReparseTarget(*target);

        if (filesystemProbe_.TargetDirectoryExists(pointsAt))
        {
            return LinkOutcome::Occupied(OccupiedDestination{linkPath, pointsAt});
        }

        if (!linkService_.RemoveReparseNode(linkPath))
        {
            return LinkOutcome::Failed(LinkFailure::CouldNotReplaceStaleLink);
        }
    }

    if (const LinkFailure refusal = linkService_.CreateLink(linkPath, addon.folderPath, linkType);
        refusal != LinkFailure::None)
    {
        return LinkOutcome::Failed(refusal);
    }

    return LinkOutcome::Success();
}

LinkOutcome LinkingEngine::Disable(const std::filesystem::path& linkPath) const
{
    if (!filesystemProbe_.IsReparsePoint(linkPath))
    {
        return LinkOutcome::Failed(LinkFailure::PathIsNotAReparsePoint);
    }

    if (!linkService_.RemoveReparseNode(linkPath))
    {
        return LinkOutcome::Failed(LinkFailure::CouldNotRemoveLink);
    }

    return LinkOutcome::Success();
}
