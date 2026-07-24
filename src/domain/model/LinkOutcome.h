#ifndef FS_ORGANIZER_DOMAIN_MODEL_LINK_OUTCOME_H
#define FS_ORGANIZER_DOMAIN_MODEL_LINK_OUTCOME_H

#include <optional>
#include <utility>

#include "domain/model/CopyConflict.h"
#include "domain/model/LinkFailure.h"
#include "domain/model/OccupiedDestination.h"

class LinkOutcome
{
public:
    [[nodiscard]] static LinkOutcome Success()
    {
        return {LinkFailure::None, std::nullopt, std::nullopt};
    }

    [[nodiscard]] static LinkOutcome Failed(const LinkFailure failure)
    {
        return {failure, std::nullopt, std::nullopt};
    }

    [[nodiscard]] static LinkOutcome Conflicted(CopyConflict conflict)
    {
        return {LinkFailure::DestinationHoldsRealFolder, std::move(conflict), std::nullopt};
    }

    [[nodiscard]] static LinkOutcome Occupied(OccupiedDestination occupation)
    {
        return {LinkFailure::DestinationHoldsLiveLink, std::nullopt, std::move(occupation)};
    }

    [[nodiscard]] bool Succeeded() const
    {
        return failure_ == LinkFailure::None;
    }

    [[nodiscard]] LinkFailure Failure() const
    {
        return failure_;
    }

    [[nodiscard]] const std::optional<CopyConflict>& Conflict() const
    {
        return conflict_;
    }

    [[nodiscard]] const std::optional<OccupiedDestination>& Occupation() const
    {
        return occupation_;
    }

private:
    LinkOutcome(const LinkFailure failure,
                std::optional<CopyConflict> conflict,
                std::optional<OccupiedDestination> occupation)
        : failure_(failure), conflict_(std::move(conflict)), occupation_(std::move(occupation))
    {
    }

    LinkFailure failure_;
    std::optional<CopyConflict> conflict_;
    std::optional<OccupiedDestination> occupation_;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_LINK_OUTCOME_H
