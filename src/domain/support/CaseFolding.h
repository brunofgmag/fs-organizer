#ifndef FS_ORGANIZER_DOMAIN_SUPPORT_CASE_FOLDING_H
#define FS_ORGANIZER_DOMAIN_SUPPORT_CASE_FOLDING_H

#include <cstddef>
#include <string>

struct ReadCharacter
{
    char32_t code = 0;
    std::size_t length = 0;
};

[[nodiscard]] inline ReadCharacter CharacterAt(const std::string& text, const std::size_t at)
{
    const auto lead = static_cast<unsigned char>(text[at]);

    if (lead < 0x80)
    {
        return {.code = lead, .length = 1};
    }

    const ReadCharacter alone{.code = lead, .length = 1};
    std::size_t length = 0;
    char32_t code = 0;

    if ((lead & 0xE0U) == 0xC0U)
    {
        length = 2;
        code = lead & 0x1FU;
    }
    else if ((lead & 0xF0U) == 0xE0U)
    {
        length = 3;
        code = lead & 0x0FU;
    }
    else if ((lead & 0xF8U) == 0xF0U)
    {
        length = 4;
        code = lead & 0x07U;
    }
    else
    {
        return alone;
    }

    if (at + length > text.size())
    {
        return alone;
    }

    for (std::size_t step = 1; step < length; ++step)
    {
        const auto continuation = static_cast<unsigned char>(text[at + step]);

        if ((continuation & 0xC0U) != 0x80U)
        {
            return alone;
        }

        code = (code << 6) | (continuation & 0x3FU);
    }

    return {.code = code, .length = length};
}

inline void AppendAsUtf8(std::string& text, const char32_t code)
{
    if (code < 0x80)
    {
        text += static_cast<char>(code);

        return;
    }

    if (code < 0x800)
    {
        text += static_cast<char>(0xC0U | (code >> 6));
        text += static_cast<char>(0x80U | (code & 0x3FU));

        return;
    }

    if (code < 0x10000)
    {
        text += static_cast<char>(0xE0U | (code >> 12));
        text += static_cast<char>(0x80U | ((code >> 6) & 0x3FU));
        text += static_cast<char>(0x80U | (code & 0x3FU));

        return;
    }

    text += static_cast<char>(0xF0U | (code >> 18));
    text += static_cast<char>(0x80U | ((code >> 12) & 0x3FU));
    text += static_cast<char>(0x80U | ((code >> 6) & 0x3FU));
    text += static_cast<char>(0x80U | (code & 0x3FU));
}

[[nodiscard]] inline bool IsEven(const char32_t code)
{
    return (code % 2) == 0;
}

[[nodiscard]] inline char32_t LoweredCodePoint(const char32_t code)
{
    if (code >= U'A' && code <= U'Z')
    {
        return code + 0x20;
    }

    if (code < 0xC0)
    {
        return code;
    }

    if ((code >= 0xC0 && code <= 0xD6) || (code >= 0xD8 && code <= 0xDE))
    {
        return code + 0x20;
    }

    if (code == 0x0130 || code == 0x0131)
    {
        return code;
    }

    if ((code >= 0x0100 && code <= 0x0137) || (code >= 0x014A && code <= 0x0177))
    {
        return IsEven(code) ? code + 1 : code;
    }

    if ((code >= 0x0139 && code <= 0x0148) || (code >= 0x0179 && code <= 0x017E))
    {
        return IsEven(code) ? code : code + 1;
    }

    if (code == 0x0178)
    {
        return 0xFF;
    }

    if (code == 0x0386)
    {
        return 0x03AC;
    }

    if (code >= 0x0388 && code <= 0x038A)
    {
        return code + 0x25;
    }

    if (code == 0x038C)
    {
        return 0x03CC;
    }

    if (code == 0x038E || code == 0x038F)
    {
        return code + 0x3F;
    }

    if ((code >= 0x0391 && code <= 0x03A1) || (code >= 0x03A3 && code <= 0x03AB))
    {
        return code + 0x20;
    }

    if (code >= 0x0400 && code <= 0x040F)
    {
        return code + 0x50;
    }

    if (code >= 0x0410 && code <= 0x042F)
    {
        return code + 0x20;
    }

    return code;
}

[[nodiscard]] inline std::string LoweredForComparison(const std::string& text)
{
    std::string lowered;
    lowered.reserve(text.size());

    for (std::size_t at = 0; at < text.size();)
    {
        const ReadCharacter read = CharacterAt(text, at);
        const char32_t code = LoweredCodePoint(read.code);

        if (read.length == 1)
        {
            lowered += static_cast<char>(code & 0xFFU);
        }
        else
        {
            AppendAsUtf8(lowered, code);
        }

        at += read.length;
    }

    return lowered;
}

#endif // FS_ORGANIZER_DOMAIN_SUPPORT_CASE_FOLDING_H
