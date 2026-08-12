#include "domain/documents/ChartRevisions.h"

#include <algorithm>
#include <cstddef>

namespace
{
    [[nodiscard]] bool TheNewerComesFirst(const PageRevision& left, const PageRevision& right)
    {
        if (left.page != right.page)
        {
            return left.page < right.page;
        }

        return left.version > right.version;
    }
}

WhichRevisionOfEachPage TheRevisionInForceOfEachPage(const std::vector<PageRevision>& pages)
{
    std::vector<PageRevision> ordered = pages;
    std::ranges::stable_sort(ordered, TheNewerComesFirst);

    WhichRevisionOfEachPage chosen;

    for (std::size_t index = 0; index < ordered.size(); ++index)
    {
        const bool itIsTheFirstOfItsPage = index == 0 || ordered[index - 1].page != ordered[index].page;

        if (itIsTheFirstOfItsPage)
        {
            chosen.inForce.push_back(ordered[index]);
            continue;
        }

        chosen.previous.push_back(ordered[index]);
    }

    return chosen;
}
