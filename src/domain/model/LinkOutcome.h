#ifndef FS_ORGANIZER_DOMAIN_MODEL_LINK_OUTCOME_H
#define FS_ORGANIZER_DOMAIN_MODEL_LINK_OUTCOME_H

#include <optional>
#include <string>
#include <utility>

#include "domain/model/CopyConflict.h"
#include "domain/model/OccupiedDestination.h"

class LinkOutcome
{
public:
    [[nodiscard]] static LinkOutcome Success()
    {
        return {true, {}, std::nullopt, std::nullopt};
    }

    [[nodiscard]] static LinkOutcome Failure(std::string message)
    {
        return {false, std::move(message), std::nullopt, std::nullopt};
    }

    [[nodiscard]] static LinkOutcome Conflicted(CopyConflict conflict)
    {
        std::string message = conflict.DestinationPath.string() + " already holds a real folder";
        return {false, std::move(message), std::move(conflict), std::nullopt};
    }

    [[nodiscard]] static LinkOutcome Occupied(OccupiedDestination occupation)
    {
        std::string message = occupation.DestinationPath.string() + " already links to "
            + occupation.ExistingTarget.string();
        return {false, std::move(message), std::nullopt, std::move(occupation)};
    }

    [[nodiscard]] bool Succeeded() const
    {
        return succeeded_;
    }

    [[nodiscard]] const std::string& Message() const
    {
        return message_;
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
    LinkOutcome(const bool succeeded,
                std::string message,
                std::optional<CopyConflict> conflict,
                std::optional<OccupiedDestination> occupation)
        : succeeded_(succeeded),
          message_(std::move(message)),
          conflict_(std::move(conflict)),
          occupation_(std::move(occupation))
    {
    }

    bool succeeded_;
    std::string message_;
    std::optional<CopyConflict> conflict_;
    std::optional<OccupiedDestination> occupation_;
};

#endif
