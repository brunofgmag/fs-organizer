#include "shared/BglAirportCodes.h"

#include <cstddef>

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

    [[nodiscard]] bool Reaches(const std::vector<std::uint8_t>& bytes, const std::size_t at, const std::size_t wide)
    {
        return at + wide <= bytes.size();
    }

    [[nodiscard]] std::uint16_t WordAt(const std::vector<std::uint8_t>& bytes, const std::size_t at)
    {
        return static_cast<std::uint16_t>(bytes[at] | bytes[at + 1] << 8);
    }

    [[nodiscard]] std::uint32_t DoubleWordAt(const std::vector<std::uint8_t>& bytes, const std::size_t at)
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

    [[nodiscard]] bool CarriesTheSignature(const std::vector<std::uint8_t>& bytes)
    {
        return Reaches(bytes, 0, kSmallestHeader) && DoubleWordAt(bytes, 0) == kSignature;
    }

    [[nodiscard]] std::size_t SectionAt(const std::vector<std::uint8_t>& bytes, const std::uint32_t section)
    {
        return DoubleWordAt(bytes, 4) + section * kSectionEntry;
    }

    [[nodiscard]] BglReading ReadRecords(const std::vector<std::uint8_t>& bytes,
                                         std::size_t at,
                                         const std::uint32_t records,
                                         std::vector<std::string>& into)
    {
        for (std::uint32_t record = 0; record < records; ++record)
        {
            if (!Reaches(bytes, at, 6))
            {
                return BglReading::ItEndsBeforeItSaysItDoes;
            }

            const std::uint16_t kind = WordAt(bytes, at);
            const std::size_t size = DoubleWordAt(bytes, at + 2);

            if (size < 6 || !Reaches(bytes, at, size))
            {
                return BglReading::ItEndsBeforeItSaysItDoes;
            }

            const IdentifierIn* identifier = IdentifierOf(kind);

            if (identifier != nullptr && identifier->at + 4 <= size)
            {
                if (std::string code = AirportCodeFrom(DoubleWordAt(bytes, at + identifier->at), identifier->shift);
                    !code.empty())
                {
                    into.push_back(std::move(code));
                }
            }

            at += size;
        }

        return BglReading::Read;
    }

    [[nodiscard]] BglReading
    ReadSubsections(const std::vector<std::uint8_t>& bytes, const std::size_t entry, std::vector<std::string>& into)
    {
        const std::uint32_t subsections = DoubleWordAt(bytes, entry + 8);
        const std::size_t subsectionsAt = DoubleWordAt(bytes, entry + 12);

        for (std::uint32_t subsection = 0; subsection < subsections; ++subsection)
        {
            const std::size_t where = subsectionsAt + subsection * kSubsectionEntry;

            if (!Reaches(bytes, where, kSubsectionEntry))
            {
                return BglReading::ItEndsBeforeItSaysItDoes;
            }

            const BglReading reading =
                ReadRecords(bytes, DoubleWordAt(bytes, where + 8), DoubleWordAt(bytes, where + 4), into);

            if (reading != BglReading::Read)
            {
                return reading;
            }
        }

        return BglReading::Read;
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

bool CouldCarryAnAirportSection(const std::vector<std::uint8_t>& head)
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

BglAirportCodes AirportCodesIn(const std::vector<std::uint8_t>& bytes)
{
    if (!CarriesTheSignature(bytes))
    {
        return {.reading = BglReading::ItCarriesNoSignature};
    }

    BglAirportCodes found;

    const std::uint32_t sections = DoubleWordAt(bytes, 20);

    for (std::uint32_t section = 0; section < sections; ++section)
    {
        const std::size_t entry = SectionAt(bytes, section);

        if (!Reaches(bytes, entry, kSectionEntry))
        {
            found.reading = BglReading::ItEndsBeforeItSaysItDoes;
            return found;
        }

        if (DoubleWordAt(bytes, entry) != kAirportSection)
        {
            continue;
        }

        found.reading = ReadSubsections(bytes, entry, found.codes);

        if (found.reading != BglReading::Read)
        {
            return found;
        }
    }

    return found;
}
