#include "infrastructure/catalog/JsonChartCatalogueParser.h"

#include <string>

#include <QtCore/QByteArray>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

namespace
{
    [[nodiscard]] std::string TextField(const QJsonObject& object, const char* key)
    {
        return object.value(QLatin1StringView(key)).toString().toStdString();
    }

    [[nodiscard]] std::vector<CatalogueEntry> EntriesIn(const QJsonObject& object)
    {
        std::vector<CatalogueEntry> entries;

        for (const QJsonValue entry : object.value(QLatin1StringView("catalogue")).toArray())
        {
            const QJsonObject fields = entry.toObject();
            std::string chartId = TextField(fields, "chart_id");

            if (!chartId.empty())
            {
                entries.push_back({.chartId = std::move(chartId),
                                   .chartType = TextField(fields, "chart_type"),
                                   .chartName = TextField(fields, "chart_name")});
            }
        }

        return entries;
    }
}

std::optional<ChartCatalogue> JsonChartCatalogueParser::Parse(const std::string_view content) const
{
    const QJsonDocument document =
        QJsonDocument::fromJson(QByteArray::fromRawData(content.data(), static_cast<qsizetype>(content.size())));

    if (!document.isObject())
    {
        return std::nullopt;
    }

    const QJsonObject object = document.object();

    return ChartCatalogue{.icao = TextField(object, "icao"), .entries = EntriesIn(object)};
}
