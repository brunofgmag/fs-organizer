#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_SCENERY_PARSER_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_SCENERY_PARSER_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "domain/ports/SceneryParser.h"

class FakeSceneryParser final : public SceneryParser
{
public:
    [[nodiscard]] SceneryCodes Parse(const std::span<const std::uint8_t> bytes) const override
    {
        ++parsed;

        const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());

        if (!text.starts_with(kSignature))
        {
            return {.reading = SceneryReading::ItCarriesNoSignature};
        }

        if (text.find(kTruncated) != std::string_view::npos)
        {
            return {.reading = SceneryReading::ItEndsBeforeItSaysItDoes};
        }

        SceneryCodes found;

        for (std::size_t at = kSignature.size(); at + 4 <= text.size(); at += 5)
        {
            const std::string_view code = text.substr(at, 4);

            if (code == kUnreadable)
            {
                found.anIdentifierDidNotDecode = true;
                continue;
            }

            found.codes.emplace_back(code);
        }

        return found;
    }

    [[nodiscard]] bool CouldCarryAnAirportSection(const std::span<const std::uint8_t> head) const override
    {
        ++prefiltered;

        return std::string_view(reinterpret_cast<const char*>(head.data()), head.size()).starts_with(kSignature);
    }

    [[nodiscard]] static std::string Carrying(const std::vector<std::string>& codes)
    {
        std::string bytes(kSignature);
        for (const std::string& code : codes)
        {
            bytes.append(code).push_back(' ');
        }

        return bytes;
    }

    [[nodiscard]] static std::string CarryingNothing()
    {
        return std::string(kSignature);
    }

    [[nodiscard]] static std::string ThatEndsEarly()
    {
        return std::string(kSignature).append(kTruncated);
    }

    static constexpr std::string_view kSignature = "BGL!";
    static constexpr std::string_view kUnreadable = "????";
    static constexpr std::string_view kTruncated = "CUT!";

    mutable std::size_t parsed = 0;
    mutable std::size_t prefiltered = 0;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_SCENERY_PARSER_H
