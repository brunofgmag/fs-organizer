#ifndef FS_ORGANIZER_TOOLS_SHARED_BGL_AIRPORT_CODES_H
#define FS_ORGANIZER_TOOLS_SHARED_BGL_AIRPORT_CODES_H

#include <cstdint>
#include <string>
#include <vector>

enum class BglReading : int
{
    Read = 0,
    ItCarriesNoSignature = 1,
    ItEndsBeforeItSaysItDoes = 2,
};

struct BglAirportCodes
{
    BglReading reading = BglReading::Read;
    std::vector<std::string> codes{};
};

[[nodiscard]] BglAirportCodes AirportCodesIn(const std::vector<std::uint8_t>& bytes);

[[nodiscard]] bool CouldCarryAnAirportSection(const std::vector<std::uint8_t>& head);

[[nodiscard]] std::string AirportCodeFrom(std::uint32_t packed, unsigned int shift);

#endif // FS_ORGANIZER_TOOLS_SHARED_BGL_AIRPORT_CODES_H
