#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_CHART_CATALOGUE_PARSER_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_CHART_CATALOGUE_PARSER_H

#include <map>
#include <string>
#include <utility>

#include "domain/ports/ChartCatalogueParser.h"

class FakeChartCatalogueParser final : public ChartCatalogueParser
{
public:
    void Answer(const std::string& content, ChartCatalogue catalogue)
    {
        answers_[content] = std::move(catalogue);
    }

    [[nodiscard]] std::optional<ChartCatalogue> Parse(const std::string_view content) const override
    {
        parsed.emplace_back(content);

        const auto known = answers_.find(std::string(content));

        if (known == answers_.end())
        {
            return std::nullopt;
        }

        return known->second;
    }

    mutable std::vector<std::string> parsed;

private:
    std::map<std::string, ChartCatalogue> answers_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_CHART_CATALOGUE_PARSER_H
