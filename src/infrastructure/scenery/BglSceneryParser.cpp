#include "infrastructure/scenery/BglSceneryParser.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace
{
    constexpr std::uint32_t kSignature = 0x19920201;
    constexpr std::uint32_t kAirportSection = 3;
    constexpr std::size_t kSectionEntry = 20;
    constexpr std::size_t kSubsectionEntry = 16;
    constexpr std::size_t kSmallestHeader = 24;
    constexpr std::uint32_t kBase = 38;
    constexpr std::uint32_t kFirstDigit = 2;
    constexpr std::uint32_t kFirstLetter = 12;

    struct IdentifierIn
    {
        std::uint16_t record = 0;
        std::size_t at = 0;
        unsigned int shift = 0;
    };

    constexpr IdentifierIn kIdentifiers[] = {
        {.record = 0x3C, .at = 40, .shift = 5},
        {.record = 0x56, .at = 40, .shift = 5},
        {.record = 0x113, .at = 76, .shift = 6},
    };

    [[nodiscard]] bool Reaches(const std::span<const std::uint8_t> bytes, const std::size_t at, const std::size_t wide)
    {
        return at + wide <= bytes.size();
    }

    [[nodiscard]] std::uint16_t WordAt(const std::span<const std::uint8_t> bytes, const std::size_t at)
    {
        return static_cast<std::uint16_t>(bytes[at] | bytes[at + 1] << 8);
    }

    [[nodiscard]] std::uint32_t DoubleWordAt(const std::span<const std::uint8_t> bytes, const std::size_t at)
    {
        return static_cast<std::uint32_t>(bytes[at]) | static_cast<std::uint32_t>(bytes[at + 1]) << 8
            | static_cast<std::uint32_t>(bytes[at + 2]) << 16 | static_cast<std::uint32_t>(bytes[at + 3]) << 24;
    }

    [[nodiscard]] const IdentifierIn* IdentifierOf(const std::uint16_t record)
    {
        for (const IdentifierIn& known : kIdentifiers)
        {
            if (known.record == record)
            {
                return &known;
            }
        }

        return nullptr;
    }

    [[nodiscard]] bool CarriesTheSignature(const std::span<const std::uint8_t> bytes)
    {
        return Reaches(bytes, 0, kSmallestHeader) && DoubleWordAt(bytes, 0) == kSignature;
    }

    [[nodiscard]] std::size_t SectionAt(const std::span<const std::uint8_t> bytes, const std::uint32_t section)
    {
        return DoubleWordAt(bytes, 4) + section * kSectionEntry;
    }

    void ReadTheIdentifier(const std::span<const std::uint8_t> bytes,
                           const std::size_t at,
                           const std::size_t size,
                           const IdentifierIn& identifier,
                           SceneryCodes& into)
    {
        if (identifier.at + 4 > size)
        {
            into.anIdentifierDidNotDecode = true;
            return;
        }

        std::string code = AirportCodeFrom(DoubleWordAt(bytes, at + identifier.at), identifier.shift);

        if (code.empty())
        {
            into.anIdentifierDidNotDecode = true;
            return;
        }

        into.codes.push_back(std::move(code));
    }

    [[nodiscard]] SceneryReading ReadRecords(const std::span<const std::uint8_t> bytes,
                                             std::size_t at,
                                             const std::uint32_t records,
                                             SceneryCodes& into)
    {
        for (std::uint32_t record = 0; record < records; ++record)
        {
            if (!Reaches(bytes, at, 6))
            {
                return SceneryReading::ItEndsBeforeItSaysItDoes;
            }

            const std::uint16_t kind = WordAt(bytes, at);
            const std::size_t size = DoubleWordAt(bytes, at + 2);

            if (size < 6 || !Reaches(bytes, at, size))
            {
                return SceneryReading::ItEndsBeforeItSaysItDoes;
            }

            if (const IdentifierIn* identifier = IdentifierOf(kind); identifier != nullptr)
            {
                ReadTheIdentifier(bytes, at, size, *identifier, into);
            }

            at += size;
        }

        return SceneryReading::Read;
    }

    [[nodiscard]] SceneryReading
    ReadSubsections(const std::span<const std::uint8_t> bytes, const std::size_t entry, SceneryCodes& into)
    {
        const std::uint32_t subsections = DoubleWordAt(bytes, entry + 8);
        const std::size_t subsectionsAt = DoubleWordAt(bytes, entry + 12);

        for (std::uint32_t subsection = 0; subsection < subsections; ++subsection)
        {
            const std::size_t where = subsectionsAt + subsection * kSubsectionEntry;

            if (!Reaches(bytes, where, kSubsectionEntry))
            {
                return SceneryReading::ItEndsBeforeItSaysItDoes;
            }

            const SceneryReading reading =
                ReadRecords(bytes, DoubleWordAt(bytes, where + 8), DoubleWordAt(bytes, where + 4), into);

            if (reading != SceneryReading::Read)
            {
                return reading;
            }
        }

        return SceneryReading::Read;
    }
}

std::string AirportCodeFrom(const std::uint32_t packed, const unsigned int shift)
{
    std::string code;

    for (std::uint32_t left = packed >> shift; left > 0; left /= kBase)
    {
        const std::uint32_t digit = left % kBase;

        if (digit < kFirstDigit)
        {
            return {};
        }

        code.insert(code.begin(),
                    digit < kFirstLetter ? static_cast<char>('0' + digit - kFirstDigit)
                                         : static_cast<char>('A' + digit - kFirstLetter));
    }

    return code;
}

bool BglSceneryParser::CouldCarryAnAirportSection(const std::span<const std::uint8_t> head) const
{
    if (!CarriesTheSignature(head))
    {
        return false;
    }

    const std::uint32_t sections = DoubleWordAt(head, 20);

    for (std::uint32_t section = 0; section < sections; ++section)
    {
        const std::size_t entry = SectionAt(head, section);

        if (!Reaches(head, entry, kSectionEntry) || DoubleWordAt(head, entry) == kAirportSection)
        {
            return true;
        }
    }

    return false;
}

SceneryCodes BglSceneryParser::Parse(const std::span<const std::uint8_t> bytes) const
{
    if (!CarriesTheSignature(bytes))
    {
        return {.reading = SceneryReading::ItCarriesNoSignature};
    }

    SceneryCodes found;

    const std::uint32_t sections = DoubleWordAt(bytes, 20);

    for (std::uint32_t section = 0; section < sections; ++section)
    {
        const std::size_t entry = SectionAt(bytes, section);

        if (!Reaches(bytes, entry, kSectionEntry))
        {
            found.reading = SceneryReading::ItEndsBeforeItSaysItDoes;
            return found;
        }

        if (DoubleWordAt(bytes, entry) != kAirportSection)
        {
            continue;
        }

        found.reading = ReadSubsections(bytes, entry, found);

        if (found.reading != SceneryReading::Read)
        {
            return found;
        }
    }

    return found;
}
