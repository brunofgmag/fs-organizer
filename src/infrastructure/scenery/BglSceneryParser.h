#ifndef FS_ORGANIZER_INFRASTRUCTURE_SCENERY_BGL_SCENERY_PARSER_H
#define FS_ORGANIZER_INFRASTRUCTURE_SCENERY_BGL_SCENERY_PARSER_H

#include <cstdint>
#include <span>
#include <string>

#include "domain/ports/SceneryParser.h"

class BglSceneryParser final : public SceneryParser
{
public:
    [[nodiscard]] SceneryCodes Parse(std::span<const std::uint8_t> bytes) const override;

    [[nodiscard]] bool CouldCarryAnAirportSection(std::span<const std::uint8_t> head) const override;
};

[[nodiscard]] std::string AirportCodeFrom(std::uint32_t packed, unsigned int shift);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SCENERY_BGL_SCENERY_PARSER_H
