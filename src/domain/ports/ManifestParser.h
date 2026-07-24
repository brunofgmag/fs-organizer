#ifndef FS_ORGANIZER_DOMAIN_PORTS_MANIFEST_PARSER_H
#define FS_ORGANIZER_DOMAIN_PORTS_MANIFEST_PARSER_H

#include <optional>
#include <string_view>

#include "domain/model/Manifest.h"

class ManifestParser
{
public:
    virtual ~ManifestParser() = default;

    [[nodiscard]] virtual std::optional<Manifest> Parse(std::string_view content) const = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_MANIFEST_PARSER_H
