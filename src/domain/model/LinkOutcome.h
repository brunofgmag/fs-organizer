#ifndef FS_ORGANIZER_DOMAIN_MODEL_LINK_OUTCOME_H
#define FS_ORGANIZER_DOMAIN_MODEL_LINK_OUTCOME_H

#include <filesystem>
#include <utility>
#include <variant>

#include "domain/model/CopyConflict.h"
#include "domain/model/FileResult.h"
#include "domain/model/LinkFailure.h"

struct OccupiedDestination
{
    std::filesystem::path destinationPath;
    std::filesystem::path existingTarget;
};

class LinkOutcome
{
public:
    [[nodiscard]] static LinkOutcome Success()
    {
        return {LinkFailure::None, std::monostate{}};
    }

    [[nodiscard]] static LinkOutcome Failed(const LinkFailure failure)
    {
        return {failure, std::monostate{}};
    }

    [[nodiscard]] static LinkOutcome Conflicted(CopyConflict conflict)
    {
        return {LinkFailure::DestinationHoldsRealFolder, std::move(conflict)};
    }

    [[nodiscard]] static LinkOutcome Occupied(OccupiedDestination occupation)
    {
        return {LinkFailure::DestinationHoldsLiveLink, std::move(occupation)};
    }

    [[nodiscard]] static LinkOutcome OfFile(const FileResult result)
    {
        return {LinkFailure::None, result};
    }

    [[nodiscard]] bool Succeeded() const
    {
        if (const FileResult* file = File(); file != nullptr)
        {
            return ::Succeeded(*file);
        }

        return failure_ == LinkFailure::None;
    }

    [[nodiscard]] LinkFailure Failure() const
    {
        return failure_;
    }

    [[nodiscard]] const CopyConflict* Conflict() const
    {
        return std::get_if<CopyConflict>(&detail_);
    }

    [[nodiscard]] const OccupiedDestination* Occupation() const
    {
        return std::get_if<OccupiedDestination>(&detail_);
    }

    [[nodiscard]] const FileResult* File() const
    {
        return std::get_if<FileResult>(&detail_);
    }

private:
    using Detail = std::variant<std::monostate, CopyConflict, OccupiedDestination, FileResult>;

    LinkOutcome(const LinkFailure failure, Detail detail) : failure_(failure), detail_(std::move(detail))
    {
    }

    LinkFailure failure_;
    Detail detail_;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_LINK_OUTCOME_H
