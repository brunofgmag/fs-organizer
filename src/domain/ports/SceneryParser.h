#ifndef FS_ORGANIZER_DOMAIN_PORTS_SCENERY_PARSER_H
#define FS_ORGANIZER_DOMAIN_PORTS_SCENERY_PARSER_H

#include <cstdint>
#include <span>

#include "domain/model/SceneryCodes.h"

class SceneryParser
{
public:
    virtual ~SceneryParser() = default;

    [[nodiscard]] virtual SceneryCodes Parse(std::span<const std::uint8_t> bytes) const = 0;

    [[nodiscard]] virtual bool CouldCarryAnAirportSection(std::span<const std::uint8_t> head) const = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_SCENERY_PARSER_H
