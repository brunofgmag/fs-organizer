#ifndef FS_ORGANIZER_INFRASTRUCTURE_CATALOG_JSON_CHART_CATALOGUE_PARSER_H
#define FS_ORGANIZER_INFRASTRUCTURE_CATALOG_JSON_CHART_CATALOGUE_PARSER_H

#include "domain/ports/ChartCatalogueParser.h"

class JsonChartCatalogueParser final : public ChartCatalogueParser
{
public:
    [[nodiscard]] std::optional<ChartCatalogue> Parse(std::string_view content) const override;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_CATALOG_JSON_CHART_CATALOGUE_PARSER_H
