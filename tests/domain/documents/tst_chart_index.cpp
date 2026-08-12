#include <QtTest/QtTest>

#include <cstddef>
#include <string>
#include <vector>

#include "domain/documents/ChartIndex.h"
#include "domain/support/PathUtils.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class ChartIndexTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheChartsOfEachAirportComeOutInAGroupOfTheirOwn();
        static void ChartsWhoseCodeWasNotDeterminedGetAGroupOfTheirOwnAndItComesLast();
        static void NoChartAppearsInTwoGroups();
        static void WithACatalogueEachChartIsNamedByItAndGroupedByItsType();
        static void TheCatalogueMeetsTheFileByTheChartIdWhichIsTheFileNameWithoutItsExtension();
        static void AnAirportWithoutACatalogueFallsIntoAFlatListWithNoTypeAndTheFileNameAsTheLabel();
        static void APageOfASidComesOutAsAPageOfThatSidAndNeverAsAChartOfItsOwn();
        static void APageWhoseSidIsMissingStaysAChartOfItsOwnInsteadOfDisappearing();
        static void TheAoiEntriesOfAnAirportBecomeOneLineCarryingEveryPage();
        static void AFileTheCatalogueDoesNotNameStillAppearsUnderNoType();
        static void ACatalogueEntryWithNoFileBesideItProducesNoLine();
        static void TheAirportSaysHowManyEntriesItsCatalogueCarriesSoTheMatchCanBeMeasured();
        static void TheCodeInTheFileNameGroupsTheChartWhenTheFolderGaveNone();
        static void OneFolderCanHoldTheChartsOfMoreThanOneAirportAndEachGetsItsGroup();
        static void TheTokenInTheFileNameBecomesTheTypeOfTheGroup();
        static void AFileWhoseTokenTheAppDoesNotKnowStaysInTheGroupWithNoType();
    };

    [[nodiscard]] ChartFile ChartWithNoCodeNamed(const std::string& name)
    {
        return {.relativePath = PathFromUtf8("Charts/" + name), .code = {}};
    }

    [[nodiscard]] ChartFile FileOf(const std::string& code, const std::string& name)
    {
        return {.relativePath = PathFromUtf8("NavDataPro/" + code + "/" + name), .code = code};
    }

    [[nodiscard]] CatalogueOfAnAirport CatalogueOf(const std::string& code, std::vector<CatalogueEntry> entries)
    {
        return {.code = code, .catalogue = {.icao = code, .entries = std::move(entries)}};
    }

    [[nodiscard]] const ChartsOfAnAirport* AirportNamed(const std::vector<ChartsOfAnAirport>& airports,
                                                        const std::string& code)
    {
        for (const ChartsOfAnAirport& airport : airports)
        {
            if (airport.code == code)
            {
                return &airport;
            }
        }

        return nullptr;
    }

    [[nodiscard]] const ChartsOfAType* TypeNamed(const ChartsOfAnAirport& airport, const std::string& type)
    {
        for (const ChartsOfAType& group : airport.types)
        {
            if (group.type == type)
            {
                return &group;
            }
        }

        return nullptr;
    }

    [[nodiscard]] std::size_t PagesIn(const std::vector<ChartsOfAnAirport>& airports)
    {
        std::size_t pages = 0;

        for (const ChartsOfAnAirport& airport : airports)
        {
            for (const ChartsOfAType& group : airport.types)
            {
                for (const ChartEntry& chart : group.charts)
                {
                    pages += chart.pages.size();
                }
            }
        }

        return pages;
    }

    void ChartIndexTest::TheChartsOfEachAirportComeOutInAGroupOfTheirOwn()
    {
        const std::vector<ChartsOfAnAirport> airports =
            ChartsGroupedByAirport({FileOf("EBBR", "1.pdf"), FileOf("EDDM", "2.pdf"), FileOf("EBBR", "3.pdf")}, {});

        QCOMPARE(airports.size(), std::size_t{2});
        QVERIFY(AirportNamed(airports, "EBBR") != nullptr);
        QVERIFY(AirportNamed(airports, "EDDM") != nullptr);
        QCOMPARE(AirportNamed(airports, "EBBR")->types.front().charts.size(), std::size_t{2});
    }

    void ChartIndexTest::ChartsWhoseCodeWasNotDeterminedGetAGroupOfTheirOwnAndItComesLast()
    {
        const std::vector<ChartsOfAnAirport> airports = ChartsGroupedByAirport(
            {{.relativePath = PathFromUtf8("Charts/approach.pdf"), .code = {}}, FileOf("EBBR", "1.pdf")}, {});

        QCOMPARE(airports.size(), std::size_t{2});
        QCOMPARE(QString::fromStdString(airports.front().code), QString("EBBR"));
        QCOMPARE(QString::fromStdString(airports.back().code), QString());
        QCOMPARE(airports.back().types.front().charts.size(), std::size_t{1});
    }

    void ChartIndexTest::NoChartAppearsInTwoGroups()
    {
        const std::vector<ChartsOfAnAirport> airports = ChartsGroupedByAirport(
            {FileOf("EBBR", "1.pdf"), FileOf("EDDM", "2.pdf"), FileOf("EBBR", "3.pdf"), FileOf("EDDM", "4.pdf")}, {});

        QCOMPARE(PagesIn(airports), std::size_t{4});
    }

    void ChartIndexTest::WithACatalogueEachChartIsNamedByItAndGroupedByItsType()
    {
        const std::vector<ChartsOfAnAirport> airports = ChartsGroupedByAirport(
            {FileOf("EBBR", "53117.pdf"), FileOf("EBBR", "53206.pdf")},
            {CatalogueOf("EBBR",
                         {{.chartId = "53117", .chartType = "AFC", .chartName = "AFC"},
                          {.chartId = "53206", .chartType = "IAC", .chartName = "ILS or LOC Y 25L"}})});

        const ChartsOfAnAirport* brussels = AirportNamed(airports, "EBBR");

        QVERIFY(brussels != nullptr);
        QVERIFY(brussels->catalogued);
        QCOMPARE(brussels->types.size(), std::size_t{2});
        QVERIFY(TypeNamed(*brussels, "AFC") != nullptr);
        QCOMPARE(QString::fromStdString(TypeNamed(*brussels, "IAC")->charts.front().name), QString("ILS or LOC Y 25L"));
    }

    void ChartIndexTest::TheCatalogueMeetsTheFileByTheChartIdWhichIsTheFileNameWithoutItsExtension()
    {
        const std::vector<ChartsOfAnAirport> airports = ChartsGroupedByAirport(
            {FileOf("EBBR", "53117.pdf")},
            {CatalogueOf("EBBR", {{.chartId = "53117", .chartType = "AFC", .chartName = "AFC"}})});

        QCOMPARE(AirportNamed(airports, "EBBR")->types.front().charts.front().pages.front(),
                 PathFromUtf8("NavDataPro/EBBR/53117.pdf"));
    }

    void ChartIndexTest::AnAirportWithoutACatalogueFallsIntoAFlatListWithNoTypeAndTheFileNameAsTheLabel()
    {
        const std::vector<ChartsOfAnAirport> airports =
            ChartsGroupedByAirport({FileOf("EBBR", "53117.pdf")}, {CatalogueOf("EDDM", {})});

        const ChartsOfAnAirport* brussels = AirportNamed(airports, "EBBR");

        QVERIFY(!brussels->catalogued);
        QCOMPARE(brussels->types.size(), std::size_t{1});
        QCOMPARE(QString::fromStdString(brussels->types.front().type), QString());
        QCOMPARE(QString::fromStdString(brussels->types.front().charts.front().name), QString("53117"));
    }

    void ChartIndexTest::APageOfASidComesOutAsAPageOfThatSidAndNeverAsAChartOfItsOwn()
    {
        const std::vector<ChartsOfAnAirport> airports = ChartsGroupedByAirport(
            {FileOf("EBBR", "10.pdf"), FileOf("EBBR", "11.pdf"), FileOf("EBBR", "12.pdf")},
            {CatalogueOf("EBBR",
                         {{.chartId = "10", .chartType = "SID", .chartName = "RNAV SID CIV RWY 19"},
                          {.chartId = "11", .chartType = "SIDPT", .chartName = "RNAV SID CIV RWY 19 p2"},
                          {.chartId = "12", .chartType = "SIDPT", .chartName = "RNAV SID CIV RWY 19 p3"}})});

        const ChartsOfAnAirport* brussels = AirportNamed(airports, "EBBR");

        QVERIFY(TypeNamed(*brussels, "SIDPT") == nullptr);
        QCOMPARE(TypeNamed(*brussels, "SID")->charts.size(), std::size_t{1});
        QCOMPARE(TypeNamed(*brussels, "SID")->charts.front().pages.size(), std::size_t{3});
        QCOMPARE(TypeNamed(*brussels, "SID")->charts.front().pages.back(), PathFromUtf8("NavDataPro/EBBR/12.pdf"));
    }

    void ChartIndexTest::APageWhoseSidIsMissingStaysAChartOfItsOwnInsteadOfDisappearing()
    {
        const std::vector<ChartsOfAnAirport> airports = ChartsGroupedByAirport(
            {FileOf("EBBR", "11.pdf")},
            {CatalogueOf("EBBR", {{.chartId = "11", .chartType = "SIDPT", .chartName = "RNAV SID CIV RWY 19 p2"}})});

        const ChartsOfAnAirport* brussels = AirportNamed(airports, "EBBR");

        QVERIFY(TypeNamed(*brussels, "SIDPT") != nullptr);
        QCOMPARE(TypeNamed(*brussels, "SIDPT")->charts.size(), std::size_t{1});
    }

    void ChartIndexTest::TheAoiEntriesOfAnAirportBecomeOneLineCarryingEveryPage()
    {
        const std::vector<ChartsOfAnAirport> airports =
            ChartsGroupedByAirport({FileOf("EBBR", "20.pdf"), FileOf("EBBR", "21.pdf"), FileOf("EBBR", "22.pdf")},
                                   {CatalogueOf("EBBR",
                                                {{.chartId = "20", .chartType = "AOI", .chartName = "1"},
                                                 {.chartId = "21", .chartType = "AOI", .chartName = "3"},
                                                 {.chartId = "22", .chartType = "AOI", .chartName = "2"}})});

        const ChartsOfAType* information = TypeNamed(*AirportNamed(airports, "EBBR"), "AOI");

        QCOMPARE(information->charts.size(), std::size_t{1});
        QCOMPARE(information->charts.front().pages.size(), std::size_t{3});
        QCOMPARE(information->charts.front().pages[1], PathFromUtf8("NavDataPro/EBBR/22.pdf"));
    }

    void ChartIndexTest::AFileTheCatalogueDoesNotNameStillAppearsUnderNoType()
    {
        const std::vector<ChartsOfAnAirport> airports = ChartsGroupedByAirport(
            {FileOf("EBBR", "53117.pdf"), FileOf("EBBR", "orphan.pdf")},
            {CatalogueOf("EBBR", {{.chartId = "53117", .chartType = "AFC", .chartName = "AFC"}})});

        const ChartsOfAnAirport* brussels = AirportNamed(airports, "EBBR");

        QCOMPARE(brussels->types.size(), std::size_t{2});
        QCOMPARE(QString::fromStdString(brussels->types.back().type), QString());
        QCOMPARE(QString::fromStdString(brussels->types.back().charts.front().name), QString("orphan"));
    }

    void ChartIndexTest::ACatalogueEntryWithNoFileBesideItProducesNoLine()
    {
        const std::vector<ChartsOfAnAirport> airports =
            ChartsGroupedByAirport({FileOf("EBBR", "53117.pdf")},
                                   {CatalogueOf("EBBR",
                                                {{.chartId = "53117", .chartType = "AFC", .chartName = "AFC"},
                                                 {.chartId = "53999", .chartType = "AGC", .chartName = "AGC"}})});

        const ChartsOfAnAirport* brussels = AirportNamed(airports, "EBBR");

        QCOMPARE(brussels->types.size(), std::size_t{1});
        QVERIFY(TypeNamed(*brussels, "AGC") == nullptr);
    }

    void ChartIndexTest::TheAirportSaysHowManyEntriesItsCatalogueCarriesSoTheMatchCanBeMeasured()
    {
        const std::vector<ChartsOfAnAirport> airports =
            ChartsGroupedByAirport({FileOf("EBBR", "53117.pdf")},
                                   {CatalogueOf("EBBR",
                                                {{.chartId = "53117", .chartType = "AFC", .chartName = "AFC"},
                                                 {.chartId = "53999", .chartType = "AGC", .chartName = "AGC"}})});

        QCOMPARE(AirportNamed(airports, "EBBR")->entriesInTheCatalogue, std::size_t{2});
    }

    void ChartIndexTest::TheCodeInTheFileNameGroupsTheChartWhenTheFolderGaveNone()
    {
        const std::vector<ChartsOfAnAirport> airports =
            ChartsGroupedByAirport({ChartWithNoCodeNamed("AD2_LFRN_VAC.pdf")}, {});

        QCOMPARE(airports.size(), std::size_t{1});
        QCOMPARE(QString::fromStdString(airports.front().code), QString("LFRN"));
    }

    void ChartIndexTest::OneFolderCanHoldTheChartsOfMoreThanOneAirportAndEachGetsItsGroup()
    {
        const std::vector<ChartsOfAnAirport> airports =
            ChartsGroupedByAirport({ChartWithNoCodeNamed("AD2_LFST_VAC.pdf"), ChartWithNoCodeNamed("AD3_LFWH_HELI.pdf"),
                                    ChartWithNoCodeNamed("AD3_LFWN_HELI.pdf")},
                                   {});

        QCOMPARE(airports.size(), std::size_t{3});
        QVERIFY(AirportNamed(airports, "LFST") != nullptr);
        QVERIFY(AirportNamed(airports, "LFWH") != nullptr);
        QVERIFY(AirportNamed(airports, "LFWN") != nullptr);
        QCOMPARE(PagesIn(airports), std::size_t{3});
    }

    void ChartIndexTest::TheTokenInTheFileNameBecomesTheTypeOfTheGroup()
    {
        const std::vector<ChartsOfAnAirport> airports =
            ChartsGroupedByAirport({ChartWithNoCodeNamed("AD2_LFST_VAC.pdf")}, {});

        const ChartsOfAnAirport* strasbourg = AirportNamed(airports, "LFST");

        QVERIFY(!strasbourg->catalogued);
        QCOMPARE(strasbourg->types.size(), std::size_t{1});
        QCOMPARE(QString::fromStdString(strasbourg->types.front().type), QString("VAC"));
        QCOMPARE(QString::fromStdString(strasbourg->types.front().charts.front().name), QString("AD2_LFST_VAC"));
    }

    void ChartIndexTest::AFileWhoseTokenTheAppDoesNotKnowStaysInTheGroupWithNoType()
    {
        const std::vector<ChartsOfAnAirport> airports =
            ChartsGroupedByAirport({ChartWithNoCodeNamed("AD2_LFST_XYZ.pdf")}, {});

        const ChartsOfAnAirport* strasbourg = AirportNamed(airports, "LFST");

        QCOMPARE(strasbourg->types.size(), std::size_t{1});
        QCOMPARE(QString::fromStdString(strasbourg->types.front().type), QString());
    }
}

QTEST_APPLESS_MAIN(ChartIndexTest)

#include "tst_chart_index.moc"
