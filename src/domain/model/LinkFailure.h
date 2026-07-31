#ifndef FS_ORGANIZER_DOMAIN_MODEL_LINK_FAILURE_H
#define FS_ORGANIZER_DOMAIN_MODEL_LINK_FAILURE_H

#include <array>
#include <cstddef>

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
    PrivilegeNotHeld = 8,
    TheOutcomeIsUnknown = 9,
};

inline constexpr std::array kAllLinkFailures{
    LinkFailure::None,
    LinkFailure::DestinationHoldsRealFolder,
    LinkFailure::DestinationHoldsLiveLink,
    LinkFailure::UnreadableLinkTarget,
    LinkFailure::CouldNotReplaceStaleLink,
    LinkFailure::CouldNotCreateLink,
    LinkFailure::PathIsNotAReparsePoint,
    LinkFailure::CouldNotRemoveLink,
    LinkFailure::PrivilegeNotHeld,
    LinkFailure::TheOutcomeIsUnknown,
};

static_assert(kAllLinkFailures.size() == static_cast<std::size_t>(LinkFailure::TheOutcomeIsUnknown) + 1,
              "Every LinkFailure belongs in kAllLinkFailures, and the last one carries the highest value.");

#endif // FS_ORGANIZER_DOMAIN_MODEL_LINK_FAILURE_H
