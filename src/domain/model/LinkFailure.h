#ifndef FS_ORGANIZER_DOMAIN_MODEL_LINK_FAILURE_H
#define FS_ORGANIZER_DOMAIN_MODEL_LINK_FAILURE_H

enum class LinkFailure : int
{
    None = 0,
    DestinationHoldsRealFolder = 1,
    DestinationHoldsLiveLink = 2,
    UnreadableLinkTarget = 3,
    CouldNotReplaceStaleLink = 4,
    CouldNotCreateLink = 5,
    PathIsNotAReparsePoint = 6,
    CouldNotRemoveLink = 7,
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_LINK_FAILURE_H
