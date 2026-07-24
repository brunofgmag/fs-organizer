#include "domain/linking/LinkingEngine.h"

LinkingEngine::LinkingEngine(LinkService& linkService, const FileOperations& fileOperations)
    : linkService_(linkService), fileOperations_(fileOperations)
{
}

LinkOutcome LinkingEngine::Enable(const Addon& addon,
                                  const std::filesystem::path& destinationRoot,
                                  const LinkType linkType) const
{
    const std::filesystem::path linkPath = destinationRoot / addon.FolderPath.filename();

    if (fileOperations_.DirectoryExists(linkPath))
    {
        if (!linkService_.IsReparsePoint(linkPath))
        {
            return LinkOutcome::Conflicted(CopyConflict{linkPath, addon.FolderPath});
        }

        const std::optional<std::filesystem::path> target = linkService_.ReadLinkTarget(linkPath);
        if (target.has_value() && fileOperations_.DirectoryExists(*target))
        {
            return LinkOutcome::Occupied(OccupiedDestination{linkPath, *target});
        }

        if (!linkService_.RemoveLink(linkPath))
        {
            return LinkOutcome::Failure("could not replace the stale link at " + linkPath.string());
        }
    }

    if (!linkService_.CreateLink(linkPath, addon.FolderPath, linkType))
    {
        return LinkOutcome::Failure("could not create the link at " + linkPath.string());
    }

    return LinkOutcome::Success();
}

LinkOutcome LinkingEngine::Disable(const std::filesystem::path& linkPath) const
{
    if (!linkService_.IsReparsePoint(linkPath))
    {
        return LinkOutcome::Failure(linkPath.string() + " is not a reparse point");
    }

    if (!linkService_.RemoveLink(linkPath))
    {
        return LinkOutcome::Failure("could not remove the link at " + linkPath.string());
    }

    return LinkOutcome::Success();
}
