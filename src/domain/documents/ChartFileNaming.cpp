#include "domain/documents/ChartFileNaming.h"

#include <cstddef>
#include <string_view>
#include <vector>

#include "domain/support/CaseFolding.h"
#include "domain/support/PathUtils.h"

namespace
{
    constexpr char kPartSeparator = '_';
    constexpr std::size_t kLengthOfACode = 4;

    struct KnownToken
    {
        std::string_view lowered{};
        bool namesADocument = false;
    };

    constexpr KnownToken kKnownTokens[] = {
        {.lowered = "apt", .namesADocument = true},
        {.lowered = "vac", .namesADocument = false},
        {.lowered = "heli", .namesADocument = false},
    };

    [[nodiscard]] std::vector<std::string> PartsOf(const std::string& stem)
    {
        std::vector<std::string> parts;
        std::size_t start = 0;

        for (std::size_t at = 0; at <= stem.size(); ++at)
        {
            if (at != stem.size() && stem[at] != kPartSeparator)
            {
                continue;
            }

            parts.push_back(stem.substr(start, at - start));
            start = at + 1;
        }

        return parts;
    }

    [[nodiscard]] bool ItCouldBeACode(const std::string& part)
    {
        if (part.size() != kLengthOfACode)
        {
            return false;
        }

        bool carriesALetter = false;

        for (const char character : part)
        {
            const bool letter = (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
            const bool digit = character >= '0' && character <= '9';

            if (!letter && !digit)
            {
                return false;
            }

            carriesALetter = carriesALetter || letter;
        }

        return carriesALetter;
    }

    [[nodiscard]] std::string InCapitals(const std::string& text)
    {
        std::string capitals = text;

        for (char& character : capitals)
        {
            if (character >= 'a' && character <= 'z')
            {
                character = static_cast<char>(character - ('a' - 'A'));
            }
        }

        return capitals;
    }

    [[nodiscard]] const KnownToken* TokenNamed(const std::string& part)
    {
        const std::string wanted = LoweredForComparison(part);

        for (const KnownToken& token : kKnownTokens)
        {
            if (token.lowered == wanted)
            {
                return &token;
            }
        }

        return nullptr;
    }
}

WhatTheFileNameSays ReadTheChartFileName(const std::filesystem::path& relativePath)
{
    const std::vector<std::string> parts = PartsOf(AsUtf8(relativePath.stem()));

    if (parts.size() < 2)
    {
        return {};
    }

    WhatTheFileNameSays said;

    if (const KnownToken* token = TokenNamed(parts.back()); token != nullptr)
    {
        said.type = InCapitals(parts.back());
        said.namesADocument = token->namesADocument;
    }

    if (const std::string& beforeTheToken = parts[parts.size() - 2]; ItCouldBeACode(beforeTheToken))
    {
        said.code = InCapitals(beforeTheToken);
    }

    return said;
}
