#include "domain/linking/LinkingEngine.h"

LinkingEngine::LinkingEngine(LinkService& linkService, const FileOperations& fileOperations)
    : linkService_(linkService), fileOperations_(fileOperations)
{
}

LinkOutcome LinkingEngine::Enable(const Addon& addon,
                                  const std::filesystem::path& destinationRoot,
                                  const LinkType linkType) const
{
    const std::filesystem::path linkPath = destinationRoot / addon.folderPath.filename();

    if (fileOperations_.EntryExistsWithoutFollowingLinks(linkPath))
    {
        if (!linkService_.IsReparsePoint(linkPath))
        {
            return LinkOutcome::Conflicted(CopyConflict{linkPath, addon.folderPath});
        }

        const std::optional<std::filesystem::path> target = linkService_.ReadLinkTarget(linkPath);
        if (!target.has_value())
        {
            return LinkOutcome::Failed(LinkFailure::UnreadableLinkTarget);
        }

        if (fileOperations_.TargetDirectoryExists(*target))
        {
            return LinkOutcome::Occupied(OccupiedDestination{linkPath, *target});
        }

        if (!linkService_.RemoveReparseNode(linkPath))
        {
            return LinkOutcome::Failed(LinkFailure::CouldNotReplaceStaleLink);
        }
    }

    if (!linkService_.CreateLink(linkPath, addon.folderPath, linkType))
    {
        return LinkOutcome::Failed(LinkFailure::CouldNotCreateLink);
    }

    return LinkOutcome::Success();
}

LinkOutcome LinkingEngine::Disable(const std::filesystem::path& linkPath) const
{
    if (!linkService_.IsReparsePoint(linkPath))
    {
        return LinkOutcome::Failed(LinkFailure::PathIsNotAReparsePoint);
    }

    if (!linkService_.RemoveReparseNode(linkPath))
    {
        return LinkOutcome::Failed(LinkFailure::CouldNotRemoveLink);
    }

    return LinkOutcome::Success();
}
