#include "domain/documents/ChartIndex.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "domain/support/CaseFolding.h"
#include "domain/support/PathUtils.h"

namespace
{
    constexpr auto kInformation = "AOI";
    constexpr auto kPageOfASid = "SIDPT";
    constexpr auto kSid = "SID";
    constexpr auto kPageMarker = " p";

    struct Page
    {
        int number = 0;
        std::filesystem::path file{};
    };

    struct EntryBeingBuilt
    {
        std::string name{};
        std::vector<Page> pages{};
    };

    struct TypeBeingBuilt
    {
        std::string type{};
        std::vector<EntryBeingBuilt> charts{};
    };

    struct FilesOfAnAirport
    {
        std::string code{};
        std::vector<std::filesystem::path> files{};
    };

    [[nodiscard]] int NumberIn(const std::string& digits)
    {
        if (digits.empty())
        {
            return 0;
        }

        int number = 0;

        for (const char digit : digits)
        {
            if (digit < '0' || digit > '9')
            {
                return 0;
            }

            number = (number * 10) + (digit - '0');
        }

        return number;
    }

    [[nodiscard]] int PageNumberIn(const std::string& chartName)
    {
        const std::size_t marker = chartName.rfind(kPageMarker);

        if (marker == std::string::npos)
        {
            return NumberIn(chartName);
        }

        return NumberIn(chartName.substr(marker + 2));
    }

    [[nodiscard]] std::string NameOfTheSidBehind(const std::string& chartName)
    {
        const std::size_t marker = chartName.rfind(kPageMarker);

        if (marker == std::string::npos || PageNumberIn(chartName) == 0)
        {
            return chartName;
        }

        return chartName.substr(0, marker);
    }

    [[nodiscard]] std::vector<FilesOfAnAirport> ByAirport(const std::vector<ChartFile>& charts)
    {
        std::vector<FilesOfAnAirport> airports;
        FilesOfAnAirport undetermined;

        for (const ChartFile& chart : charts)
        {
            if (chart.code.empty())
            {
                undetermined.files.push_back(chart.relativePath);
                continue;
            }

            const auto known = std::ranges::find_if(airports,
                                                    [&chart](const FilesOfAnAirport& airport)
                                                    {
                                                        return airport.code == chart.code;
                                                    });

            if (known == airports.end())
            {
                airports.push_back({.code = chart.code, .files = {chart.relativePath}});
                continue;
            }

            known->files.push_back(chart.relativePath);
        }

        if (!undetermined.files.empty())
        {
            airports.push_back(std::move(undetermined));
        }

        return airports;
    }

    [[nodiscard]] const ChartCatalogue* CatalogueOf(const std::string& code,
                                                    const std::vector<CatalogueOfAnAirport>& catalogues)
    {
        for (const CatalogueOfAnAirport& catalogue : catalogues)
        {
            if (LoweredForComparison(catalogue.code) == LoweredForComparison(code))
            {
                return &catalogue.catalogue;
            }
        }

        return nullptr;
    }

    [[nodiscard]] const std::filesystem::path* FileNamed(const std::vector<std::filesystem::path>& files,
                                                         const std::string& chartId)
    {
        const std::string wanted = LoweredForComparison(chartId);

        for (const std::filesystem::path& file : files)
        {
            if (LoweredForComparison(AsUtf8(file.stem())) == wanted)
            {
                return &file;
            }
        }

        return nullptr;
    }

    [[nodiscard]] TypeBeingBuilt& GroupOfType(std::vector<TypeBeingBuilt>& types, const std::string& type)
    {
        const auto known = std::ranges::find_if(types,
                                                [&type](const TypeBeingBuilt& group)
                                                {
                                                    return group.type == type;
                                                });

        if (known != types.end())
        {
            return *known;
        }

        types.push_back({.type = type, .charts = {}});

        return types.back();
    }

    [[nodiscard]] EntryBeingBuilt*
    ChartNamed(std::vector<TypeBeingBuilt>& types, const std::string& type, const std::string& name)
    {
        for (TypeBeingBuilt& group : types)
        {
            if (group.type != type)
            {
                continue;
            }

            for (EntryBeingBuilt& chart : group.charts)
            {
                if (chart.name == name)
                {
                    return &chart;
                }
            }
        }

        return nullptr;
    }

    void
    PlaceTheChart(std::vector<TypeBeingBuilt>& types, const CatalogueEntry& entry, const std::filesystem::path& file)
    {
        const Page page{.number = PageNumberIn(entry.chartName), .file = file};

        if (entry.chartType != kInformation)
        {
            GroupOfType(types, entry.chartType).charts.push_back({.name = entry.chartName, .pages = {page}});

            return;
        }

        TypeBeingBuilt& information = GroupOfType(types, entry.chartType);

        if (information.charts.empty())
        {
            information.charts.push_back({.name = {}, .pages = {}});
        }

        information.charts.front().pages.push_back(page);
    }

    void PlaceThePageOfASid(std::vector<TypeBeingBuilt>& types,
                            const CatalogueEntry& entry,
                            const std::filesystem::path& file)
    {
        const Page page{.number = PageNumberIn(entry.chartName), .file = file};
        EntryBeingBuilt* sid = ChartNamed(types, kSid, NameOfTheSidBehind(entry.chartName));

        if (sid == nullptr)
        {
            GroupOfType(types, entry.chartType).charts.push_back({.name = entry.chartName, .pages = {page}});

            return;
        }

        sid->pages.push_back(page);
    }

    [[nodiscard]] std::vector<TypeBeingBuilt> WhatTheCatalogueNames(const FilesOfAnAirport& airport,
                                                                    const ChartCatalogue& catalogue,
                                                                    std::vector<std::string>& placed)
    {
        std::vector<TypeBeingBuilt> types;

        for (const CatalogueEntry& entry : catalogue.entries)
        {
            const std::filesystem::path* file = FileNamed(airport.files, entry.chartId);

            if (file == nullptr || entry.chartType == kPageOfASid)
            {
                continue;
            }

            PlaceTheChart(types, entry, *file);
            placed.push_back(LoweredForComparison(AsUtf8(file->stem())));
        }

        for (const CatalogueEntry& entry : catalogue.entries)
        {
            const std::filesystem::path* file = FileNamed(airport.files, entry.chartId);

            if (file == nullptr || entry.chartType != kPageOfASid)
            {
                continue;
            }

            PlaceThePageOfASid(types, entry, *file);
            placed.push_back(LoweredForComparison(AsUtf8(file->stem())));
        }

        return types;
    }

    void AddWhatTheCatalogueDoesNotName(std::vector<TypeBeingBuilt>& types,
                                        const FilesOfAnAirport& airport,
                                        const std::vector<std::string>& placed)
    {
        for (const std::filesystem::path& file : airport.files)
        {
            const std::string stem = AsUtf8(file.stem());

            if (std::ranges::find(placed, LoweredForComparison(stem)) != placed.end())
            {
                continue;
            }

            GroupOfType(types, {}).charts.push_back({.name = stem, .pages = {{.number = 0, .file = file}}});
        }
    }

    [[nodiscard]] std::vector<ChartsOfAType> Settled(std::vector<TypeBeingBuilt>& types)
    {
        std::vector<ChartsOfAType> settled;
        settled.reserve(types.size());

        for (TypeBeingBuilt& group : types)
        {
            ChartsOfAType charts{.type = group.type, .charts = {}};

            for (EntryBeingBuilt& chart : group.charts)
            {
                std::ranges::stable_sort(chart.pages,
                                         [](const Page& left, const Page& right)
                                         {
                                             return left.number < right.number;
                                         });

                ChartEntry entry{.name = chart.name, .pages = {}};
                entry.pages.reserve(chart.pages.size());

                for (const Page& page : chart.pages)
                {
                    entry.pages.push_back(page.file);
                }

                charts.charts.push_back(std::move(entry));
            }

            settled.push_back(std::move(charts));
        }

        return settled;
    }

    [[nodiscard]] ChartsOfAnAirport TheFlatListOf(const FilesOfAnAirport& airport)
    {
        std::vector<TypeBeingBuilt> types;

        AddWhatTheCatalogueDoesNotName(types, airport, {});

        return {.code = airport.code, .catalogued = false, .entriesInTheCatalogue = 0, .types = Settled(types)};
    }

    [[nodiscard]] ChartsOfAnAirport TheCataloguedListOf(const FilesOfAnAirport& airport,
                                                        const ChartCatalogue& catalogue)
    {
        std::vector<std::string> placed;
        std::vector<TypeBeingBuilt> types = WhatTheCatalogueNames(airport, catalogue, placed);

        AddWhatTheCatalogueDoesNotName(types, airport, placed);

        return {.code = airport.code,
                .catalogued = true,
                .entriesInTheCatalogue = catalogue.entries.size(),
                .types = Settled(types)};
    }
}

std::vector<ChartsOfAnAirport> ChartsGroupedByAirport(const std::vector<ChartFile>& charts,
                                                      const std::vector<CatalogueOfAnAirport>& catalogues)
{
    std::vector<ChartsOfAnAirport> grouped;

    for (const FilesOfAnAirport& airport : ByAirport(charts))
    {
        const ChartCatalogue* catalogue = CatalogueOf(airport.code, catalogues);

        if (catalogue == nullptr || airport.code.empty())
        {
            grouped.push_back(TheFlatListOf(airport));
            continue;
        }

        grouped.push_back(TheCataloguedListOf(airport, *catalogue));
    }

    return grouped;
}
