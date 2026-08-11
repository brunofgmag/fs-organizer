#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_XML_ESCAPING_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_XML_ESCAPING_H

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

[[nodiscard]] inline std::string UnescapedXmlText(const std::string_view text)
{
    constexpr std::pair<std::string_view, char> entities[] = {
        {"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};

    std::string plain;
    plain.reserve(text.size());

    for (std::size_t at = 0; at < text.size();)
    {
        const auto entity =
            std::ranges::find_if(entities,
                                 [text, at](const std::pair<std::string_view, char>& candidate)
                                 {
                                     return text.compare(at, candidate.first.size(), candidate.first) == 0;
                                 });

        if (entity == std::ranges::end(entities))
        {
            plain.push_back(text[at]);
            ++at;

            continue;
        }

        plain.push_back(entity->second);
        at += entity->first.size();
    }

    return plain;
}

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_XML_ESCAPING_H
