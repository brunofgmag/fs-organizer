#include "domain/journal/LinksTheAppMade.h"

#include <map>
#include <ranges>
#include <string>

#include "domain/support/PathUtils.h"

namespace
{
    [[nodiscard]] bool TakesTheLinkAway(const OperationKind kind)
    {
        return kind == OperationKind::DisableAddon || kind == OperationKind::RemoveBrokenLink;
    }

    void FollowTheMove(std::map<std::string, LinkTheAppMade>& made, const OperationRecord& record)
    {
        const std::string moved = ComparablePath(record.source);

        for (LinkTheAppMade& link : made | std::views::values)
        {
            if (ComparablePath(link.libraryCopy) == moved)
            {
                link.libraryCopy = record.target;
            }
        }
    }
}

std::vector<LinkTheAppMade> WhereTheAppMadeLinks(const std::vector<OperationRecord>& history)
{
    std::map<std::string, LinkTheAppMade> made;

    for (const OperationRecord& record : history)
    {
        if (!Succeeded(record.outcome))
        {
            continue;
        }

        if (CreatesALink(record.kind))
        {
            made.insert_or_assign(ComparablePath(record.target),
                                  LinkTheAppMade{.place = record.target, .libraryCopy = record.source});
        }
        else if (TakesTheLinkAway(record.kind))
        {
            made.erase(ComparablePath(record.target));
        }
        else if (record.kind == OperationKind::MoveAddon)
        {
            FollowTheMove(made, record);
        }
    }

    std::vector<LinkTheAppMade> links;
    links.reserve(made.size());

    for (const LinkTheAppMade& link : made | std::views::values)
    {
        links.push_back(link);
    }

    return links;
}
