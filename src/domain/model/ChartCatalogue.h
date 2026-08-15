#ifndef FS_ORGANIZER_DOMAIN_MODEL_CHART_CATALOGUE_H
#define FS_ORGANIZER_DOMAIN_MODEL_CHART_CATALOGUE_H

#include <string>
#include <vector>

struct CatalogueEntry
{
    std::string chartId{};
    std::string chartType{};
    std::string chartName{};
};

struct ChartCatalogue
{
    std::string icao{};
    std::vector<CatalogueEntry> entries{};
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_CHART_CATALOGUE_H
