#ifndef FS_ORGANIZER_DOMAIN_PORTS_CHART_CATALOGUE_PARSER_H
#define FS_ORGANIZER_DOMAIN_PORTS_CHART_CATALOGUE_PARSER_H

#include <optional>
#include <string_view>

#include "domain/model/ChartCatalogue.h"

class ChartCatalogueParser
{
public:
    virtual ~ChartCatalogueParser() = default;

    [[nodiscard]] virtual std::optional<ChartCatalogue> Parse(std::string_view content) const = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_CHART_CATALOGUE_PARSER_H
