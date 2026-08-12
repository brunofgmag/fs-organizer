#include <QtTest/QtTest>

#include <cstddef>
#include <optional>

#include "infrastructure/catalog/JsonChartCatalogueParser.h"

namespace
{
    class JsonChartCatalogueParserTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ACatalogueExposesTheAirportAndEveryEntryTheIndexNeeds();
        static void TheEntriesComeOutInTheOrderTheFileWroteThem();
        static void AnEntryWithoutAChartIdIsNoEntryBecauseNothingCouldOpenIt();
        static void ContentThatIsNotAJsonObjectIsNotACatalogue();
        static void ACatalogueWithoutTheListIsStillACatalogueAndCarriesNothing();
        static void AByteOrderMarkDoesNotHideTheCatalogue();
    };
}

void JsonChartCatalogueParserTest::ACatalogueExposesTheAirportAndEveryEntryTheIndexNeeds()
{
    const JsonChartCatalogueParser parser;

    const std::optional<ChartCatalogue> catalogue = parser.Parse(R"({
      "airport_id": "15168",
      "icao": "EBBR",
      "iata": "BRU",
      "catalogue": [
        {"chart_id": "53117", "chart_type": "AFC", "chart_name": "AFC", "chart_size": "993154"},
        {"chart_id": "53206", "chart_type": "IAC", "chart_name": "ILS or LOC Y 25L", "geo_chart": "1"}
      ]
    })");

    QVERIFY(catalogue.has_value());
    QCOMPARE(catalogue->icao, std::string("EBBR"));
    QCOMPARE(catalogue->entries.size(), std::size_t{2});
    QCOMPARE(catalogue->entries.front().chartId, std::string("53117"));
    QCOMPARE(catalogue->entries.front().chartType, std::string("AFC"));
    QCOMPARE(catalogue->entries.back().chartName, std::string("ILS or LOC Y 25L"));
}

void JsonChartCatalogueParserTest::TheEntriesComeOutInTheOrderTheFileWroteThem()
{
    const JsonChartCatalogueParser parser;

    const std::optional<ChartCatalogue> catalogue = parser.Parse(R"({
      "icao": "EDDM",
      "catalogue": [
        {"chart_id": "2960", "chart_type": "AOI", "chart_name": "4"},
        {"chart_id": "2957", "chart_type": "AOI", "chart_name": "1"}
      ]
    })");

    QCOMPARE(catalogue->entries.front().chartId, std::string("2960"));
    QCOMPARE(catalogue->entries.back().chartId, std::string("2957"));
}

void JsonChartCatalogueParserTest::AnEntryWithoutAChartIdIsNoEntryBecauseNothingCouldOpenIt()
{
    const JsonChartCatalogueParser parser;

    const std::optional<ChartCatalogue> catalogue = parser.Parse(R"({
      "icao": "EBBR",
      "catalogue": [
        {"chart_type": "AFC", "chart_name": "AFC"},
        {"chart_id": "53206", "chart_type": "AGC", "chart_name": "AGC"}
      ]
    })");

    QCOMPARE(catalogue->entries.size(), std::size_t{1});
    QCOMPARE(catalogue->entries.front().chartId, std::string("53206"));
}

void JsonChartCatalogueParserTest::ContentThatIsNotAJsonObjectIsNotACatalogue()
{
    const JsonChartCatalogueParser parser;

    QVERIFY(!parser.Parse("[]").has_value());
    QVERIFY(!parser.Parse("").has_value());
    QVERIFY(!parser.Parse("not json at all").has_value());
}

void JsonChartCatalogueParserTest::ACatalogueWithoutTheListIsStillACatalogueAndCarriesNothing()
{
    const JsonChartCatalogueParser parser;

    const std::optional<ChartCatalogue> catalogue = parser.Parse(R"({"icao": "EBBR"})");

    QVERIFY(catalogue.has_value());
    QVERIFY(catalogue->entries.empty());
}

void JsonChartCatalogueParserTest::AByteOrderMarkDoesNotHideTheCatalogue()
{
    const JsonChartCatalogueParser parser;

    const std::optional<ChartCatalogue> catalogue = parser.Parse("\xEF\xBB\xBF{\"icao\": \"EBBR\", \"catalogue\": []}");

    QVERIFY(catalogue.has_value());
    QCOMPARE(catalogue->icao, std::string("EBBR"));
}

QTEST_APPLESS_MAIN(JsonChartCatalogueParserTest)

#include "tst_json_chart_catalogue_parser.moc"
