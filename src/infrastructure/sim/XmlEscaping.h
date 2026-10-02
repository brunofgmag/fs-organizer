#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_XML_ESCAPING_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_XML_ESCAPING_H

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

struct XmlCharacterReference
{
    char32_t codePoint = 0;
    std::size_t length = 0;
};

[[nodiscard]] inline std::optional<XmlCharacterReference> NumericReferenceAt(const std::string_view text,
                                                                             const std::size_t at)
{
    constexpr char32_t kLastCodePoint = 0x10FFFF;
    constexpr std::size_t kMostDigits = 8;

    if (text.compare(at, 2, "&#") != 0)
    {
        return std::nullopt;
    }

    const bool hexadecimal = at + 2 < text.size() && text[at + 2] == 'x';
    const std::string_view alphabet = hexadecimal ? "0123456789abcdefABCDEF" : "0123456789";
    const char32_t radix = hexadecimal ? 16 : 10;
    const std::size_t first = at + (hexadecimal ? 3 : 2);
    const std::size_t end = text.find(';', first);

    if (end == std::string_view::npos || end - first > kMostDigits)
    {
        return std::nullopt;
    }

    char32_t codePoint = 0;

    for (const char digit : text.substr(first, end - first))
    {
        const std::size_t found = alphabet.find(digit);

        if (found == std::string_view::npos)
        {
            return std::nullopt;
        }

        const std::size_t value = found >= radix ? found - 6 : found;

        codePoint = codePoint * radix + static_cast<char32_t>(value);
    }

    const bool isSurrogate = codePoint >= 0xD800 && codePoint <= 0xDFFF;

    if (codePoint == 0 || codePoint > kLastCodePoint || isSurrogate)
    {
        return std::nullopt;
    }

    return XmlCharacterReference{.codePoint = codePoint, .length = end + 1 - at};
}

inline void AppendUtf8(std::string& text, const char32_t codePoint)
{
    if (codePoint < 0x80)
    {
        text.push_back(static_cast<char>(codePoint));
    }
    else if (codePoint < 0x800)
    {
        text.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    else if (codePoint < 0x10000)
    {
        text.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        text.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    else
    {
        text.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        text.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

[[nodiscard]] inline std::string UnescapedXmlText(const std::string_view text)
{
    constexpr std::pair<std::string_view, char> entities[] = {
        {"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};

    std::string plain;
    plain.reserve(text.size());

    for (std::size_t at = 0; at < text.size();)
    {
        if (const std::optional<XmlCharacterReference> reference = NumericReferenceAt(text, at); reference.has_value())
        {
            AppendUtf8(plain, reference->codePoint);
            at += reference->length;

            continue;
        }

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

[[nodiscard]] inline std::string EscapedXmlText(const std::string_view text)
{
    std::string escaped;
    escaped.reserve(text.size());

    for (const char letter : text)
    {
        switch (letter)
        {
        case '&': escaped.append("&amp;"); break;
        case '<': escaped.append("&lt;"); break;
        case '>': escaped.append("&gt;"); break;
        default: escaped.push_back(letter); break;
        }
    }

    return escaped;
}

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_XML_ESCAPING_H
