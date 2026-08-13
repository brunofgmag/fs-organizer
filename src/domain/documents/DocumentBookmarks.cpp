#include "domain/documents/DocumentBookmarks.h"

std::optional<std::size_t> TheSectionHolding(const std::vector<DocumentSection>& sections, const int page)
{
    std::optional<std::size_t> holding;

    for (std::size_t index = 0; index < sections.size(); ++index)
    {
        if (sections[index].page > page)
        {
            continue;
        }

        if (!holding.has_value() || sections[*holding].page < sections[index].page)
        {
            holding = index;
        }
    }

    return holding;
}
