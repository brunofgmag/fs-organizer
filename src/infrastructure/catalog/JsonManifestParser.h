#ifndef FS_ORGANIZER_INFRASTRUCTURE_CATALOG_JSON_MANIFEST_PARSER_H
#define FS_ORGANIZER_INFRASTRUCTURE_CATALOG_JSON_MANIFEST_PARSER_H

#include "domain/ports/ManifestParser.h"

class JsonManifestParser final : public ManifestParser
{
public:
    [[nodiscard]] std::optional<Manifest> Parse(std::string_view content) const override;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_CATALOG_JSON_MANIFEST_PARSER_H
