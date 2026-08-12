#ifndef FS_ORGANIZER_DOMAIN_DOCUMENTS_CHART_INDEX_H
#define FS_ORGANIZER_DOMAIN_DOCUMENTS_CHART_INDEX_H

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "domain/model/ChartCatalogue.h"

struct ChartFile
{
    std::filesystem::path relativePath{};
    std::string code{};
};

enum class ChartRevision : int
{
    InForce = 0,
    Previous = 1,
};

struct ChartEntry
{
    std::string name{};
    ChartRevision revision = ChartRevision::InForce;
    std::vector<std::filesystem::path> pages{};
};

struct ChartsOfAType
{
    std::string type{};
    std::vector<ChartEntry> charts{};
};

struct ChartsOfAnAirport
{
    std::string code{};
    bool catalogued = false;
    std::size_t entriesInTheCatalogue = 0;
    std::vector<ChartsOfAType> types{};
};

struct CatalogueOfAnAirport
{
    std::string code{};
    ChartCatalogue catalogue{};
};

struct ChartVersion
{
    std::filesystem::path file{};
    long long version = 0;
};

[[nodiscard]] std::vector<ChartsOfAnAirport> ChartsGroupedByAirport(const std::vector<ChartFile>& charts,
                                                                    const std::vector<CatalogueOfAnAirport>& catalogues,
                                                                    const std::vector<ChartVersion>& versions = {});

[[nodiscard]] std::vector<std::filesystem::path>
FilesOfARepeatedPage(const std::vector<ChartFile>& charts, const std::vector<CatalogueOfAnAirport>& catalogues);

#endif // FS_ORGANIZER_DOMAIN_DOCUMENTS_CHART_INDEX_H
