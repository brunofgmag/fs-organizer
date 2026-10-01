#include "domain/journal/LinksTheAppMade.h"

#include <ranges>

#include "domain/support/PathUtils.h"

namespace
{
    [[nodiscard]] bool TakesTheLinkAway(const OperationKind kind)
    {
        return kind == OperationKind::DisableAddon || kind == OperationKind::RemoveBrokenLink;
    }
}

void LinksTheAppMadeSoFar::Fold(const OperationRecord& record)
{
    if (!Succeeded(record.outcome))
    {
        return;
    }

    if (CreatesALink(record.kind))
    {
        made_.insert_or_assign(ComparablePath(record.target),
                               Made{.link = LinkTheAppMade{.place = record.target, .libraryCopy = record.source},
                                    .comparableCopy = ComparablePath(record.source)});
    }
    else if (TakesTheLinkAway(record.kind))
    {
        made_.erase(ComparablePath(record.target));
    }
    else if (record.kind == OperationKind::MoveAddon)
    {
        FollowTheMove(record);
    }
}

void LinksTheAppMadeSoFar::FollowTheMove(const OperationRecord& record)
{
    const std::string moved = ComparablePath(record.source);
    const std::string arrivedAt = ComparablePath(record.target);

    for (Made& made : made_ | std::views::values)
    {
        if (made.comparableCopy == moved)
        {
            made.link.libraryCopy = record.target;
            made.comparableCopy = arrivedAt;
        }
    }
}

std::vector<LinkTheAppMade> LinksTheAppMadeSoFar::Links() const
{
    std::vector<LinkTheAppMade> links;
    links.reserve(made_.size());

    for (const Made& made : made_ | std::views::values)
    {
        links.push_back(made.link);
    }

    return links;
}

std::vector<LinkTheAppMade> WhereTheAppMadeLinks(const std::vector<OperationRecord>& history)
{
    LinksTheAppMadeSoFar made;

    for (const OperationRecord& record : history)
    {
        made.Fold(record);
    }

    return made.Links();
}
