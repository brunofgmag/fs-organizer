#include <QtTest/QtTest>

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include "application/DocumentService.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakeChartCatalogueParser.h"
#include "tests/doubles/FakeChartVersions.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class DocumentServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheIndexFiltersByPdfAndOnlyByPdf();
        static void APdfUnderAFolderNamedAfterACodeTheAddonCarriesIsACharted();
        static void AnAddonWhoseCodeWasNeverExtractedProducesNoChartByCode();
        static void AFolderNamedDocsNeverBecomesAnAirport();
        static void TheCatalogueBesideTheChartsNamesThemAndGroupsThemByType();
        static void AnAddonWithoutACatalogueFallsIntoTheFlatList();
        static void AnAddonWithNoPdfCarriesNoDocumentationAtAll();
        static void TheSweepAnswersPerAddonItWasGiven();
        static void TheSweepReportsProgressAndStopsWhereItIsToldTo();
        static void TheSweepHandsOverEachAddonAsItFinishesReadingIt();
        static void AnAddonTheProbeCannotWalkSaysSoInsteadOfSayingItHasNothing();
        static void OnlyTheChartsOfAPageThatRepeatsHaveTheirVersionRead();
        static void TheInformationLineCarriesTheNewestRevisionOfEachPage();
        static void ASecondSweepOverTheSameTreeReadsNoCatalogueAndNoChartVersion();
        static void ChangingTheSizeOfOnePdfReclassifiesThatAddonAndNoOtherOne();
        static void ChangingTheWriteTimeOfOnePdfAloneReclassifiesThatAddonAndNoOtherOne();
        static void ChangingTheWriteTimeOfAFileThatIsNoPdfReclassifiesThatAddonAndNoOtherOne();
        static void TheDigestIsTakenFromTheWalkAndAsksNoFileForItsWriteTime();
        static void ChangedAirportCodesReclassifyTheAddon();
        static void AnIndexKeptBeforeTheDigestExistedReusesNothing();
        static void AnAddonThatMovedToAnotherFolderIsNotTakenForTheOneThatWasIndexed();
        static void AnAddonTheProbeCouldNotWalkIsNeverReused();
    };

    const std::filesystem::path kLibrary = PathFromUtf8("D:/Library/Sceneries");
    const LibraryId kLibraryId = "library-1";

    [[nodiscard]] AddonId Named(const std::string& folderName)
    {
        return {.libraryId = kLibraryId, .folderName = folderName};
    }

    [[nodiscard]] std::filesystem::path FolderOf(const std::string& folderName)
    {
        return PathUnder(kLibrary, PathFromUtf8(folderName));
    }

    [[nodiscard]] AddonToRead ToRead(const std::string& folderName)
    {
        return {.addon = Named(folderName), .folder = FolderOf(folderName), .itIsNavigationData = false};
    }

    [[nodiscard]] const ChartsOfAnAirport* AirportNamed(const DocumentsOfAnAddon& documents, const std::string& code)
    {
        for (const ChartsOfAnAirport& airport : documents.airports)
        {
            if (airport.code == code)
            {
                return &airport;
            }
        }

        return nullptr;
    }

    const std::string kTheCatalogueOfBrussels = "the catalogue of Brussels";

    [[nodiscard]] ChartCatalogue TheBrusselsCatalogue()
    {
        return {.icao = "EBBR",
                .entries = {{.chartId = "53117", .chartType = "AFC", .chartName = "AFC"},
                            {.chartId = "53206", .chartType = "IAC", .chartName = "ILS or LOC Y 25L"}}};
    }

    void DocumentServiceTest::TheIndexFiltersByPdfAndOnlyByPdf()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("addon") / "Manual.pdf");
        fileSystem.AddFile(FolderOf("addon") / "layout.json");
        fileSystem.AddFile(FolderOf("addon") / "html_ui" / "index.html");
        fileSystem.AddFile(FolderOf("addon") / "notes.txt");

        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const FakeChartVersions chartVersions;
        const DocumentService service(probe, catalogueParser, chartVersions);

        const DocumentsOfAnAddon documents = service.DocumentsOf(Named("addon"), FolderOf("addon"), {});

        QCOMPARE(documents.documents.size(), std::size_t{1});
        QCOMPARE(documents.documents.front(), PathFromUtf8("Manual.pdf"));
    }

    void DocumentServiceTest::APdfUnderAFolderNamedAfterACodeTheAddonCarriesIsACharted()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("addon") / "Manual.pdf");
        fileSystem.AddFile(FolderOf("addon") / "NavDataPro" / "EBBR" / "53117.pdf");

        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const FakeChartVersions chartVersions;
        const DocumentService service(probe, catalogueParser, chartVersions);

        const DocumentsOfAnAddon documents = service.DocumentsOf(Named("addon"), FolderOf("addon"), {"EBBR"});

        QCOMPARE(documents.documents.size(), std::size_t{1});
        QCOMPARE(documents.airports.size(), std::size_t{1});
        QCOMPARE(QString::fromStdString(documents.airports.front().code), QString("EBBR"));
    }

    void DocumentServiceTest::AnAddonWhoseCodeWasNeverExtractedProducesNoChartByCode()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("addon") / "NavDataPro" / "EBBR" / "53117.pdf");

        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const FakeChartVersions chartVersions;
        const DocumentService service(probe, catalogueParser, chartVersions);

        const DocumentsOfAnAddon documents = service.DocumentsOf(Named("addon"), FolderOf("addon"), {});

        QVERIFY(documents.airports.empty());
        QCOMPARE(documents.documents.size(), std::size_t{1});
    }

    void DocumentServiceTest::AFolderNamedDocsNeverBecomesAnAirport()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("addon") / "DOCS" / "handbook.pdf");

        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const FakeChartVersions chartVersions;
        const DocumentService service(probe, catalogueParser, chartVersions);

        const DocumentsOfAnAddon documents = service.DocumentsOf(Named("addon"), FolderOf("addon"), {"EBBR"});

        QVERIFY(documents.airports.empty());
        QCOMPARE(documents.documents.size(), std::size_t{1});
    }

    void DocumentServiceTest::TheCatalogueBesideTheChartsNamesThemAndGroupsThemByType()
    {
        InMemoryFileSystem fileSystem;
        const std::filesystem::path beside = FolderOf("addon") / "NavDataPro" / "EBBR";
        fileSystem.AddFile(beside / "53117.pdf");
        fileSystem.AddFile(beside / "53206.pdf");
        fileSystem.AddFileWithContents(beside / "catalogue.json", kTheCatalogueOfBrussels);

        const FakeFilesystemProbe probe(fileSystem);
        FakeChartCatalogueParser catalogueParser;
        catalogueParser.Answer(kTheCatalogueOfBrussels, TheBrusselsCatalogue());

        const FakeChartVersions chartVersions;
        const DocumentService service(probe, catalogueParser, chartVersions);

        const DocumentsOfAnAddon documents = service.DocumentsOf(Named("addon"), FolderOf("addon"), {"EBBR"});
        const ChartsOfAnAirport* brussels = AirportNamed(documents, "EBBR");

        QVERIFY(brussels != nullptr);
        QVERIFY(brussels->catalogued);
        QCOMPARE(brussels->types.size(), std::size_t{2});
        QCOMPARE(QString::fromStdString(brussels->types.back().charts.front().name), QString("ILS or LOC Y 25L"));
        QVERIFY(documents.documents.empty());
    }

    void DocumentServiceTest::AnAddonWithoutACatalogueFallsIntoTheFlatList()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("addon") / "NavDataPro" / "EBBR" / "53117.pdf");

        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const FakeChartVersions chartVersions;
        const DocumentService service(probe, catalogueParser, chartVersions);

        const DocumentsOfAnAddon documents = service.DocumentsOf(Named("addon"), FolderOf("addon"), {"EBBR"});
        const ChartsOfAnAirport* brussels = AirportNamed(documents, "EBBR");

        QVERIFY(!brussels->catalogued);
        QCOMPARE(QString::fromStdString(brussels->types.front().type), QString());
        QCOMPARE(QString::fromStdString(brussels->types.front().charts.front().name), QString("53117"));
    }

    void DocumentServiceTest::AnAddonWithNoPdfCarriesNoDocumentationAtAll()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("addon") / "manifest.json");

        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const FakeChartVersions chartVersions;
        const DocumentService service(probe, catalogueParser, chartVersions);

        const DocumentsOfAnAddon documents = service.DocumentsOf(Named("addon"), FolderOf("addon"), {"EBBR"});

        QVERIFY(documents.documents.empty());
        QVERIFY(documents.airports.empty());
        QVERIFY(documents.itWasWalked);
    }

    const std::string kTheCatalogueOfMunich = "the catalogue of Munich";

    [[nodiscard]] ChartCatalogue TheMunichCatalogue()
    {
        return {.icao = "EDDM",
                .entries = {{.chartId = "1", .chartType = "AOI", .chartName = "1"},
                            {.chartId = "2", .chartType = "AOI", .chartName = "1"},
                            {.chartId = "3", .chartType = "AOI", .chartName = "2"}}};
    }

    struct AnAirportOfRepeatedPages
    {
        InMemoryFileSystem fileSystem;
        FakeChartCatalogueParser catalogueParser;
        FakeChartVersions chartVersions;

        AnAirportOfRepeatedPages()
        {
            const std::filesystem::path beside = FolderOf("addon") / "NavDataPro" / "EDDM";

            for (const std::string& chart : {"1.pdf", "2.pdf", "3.pdf"})
            {
                fileSystem.AddFile(beside / chart);
            }

            fileSystem.AddFileWithContents(beside / "catalogue.json", kTheCatalogueOfMunich);
            catalogueParser.Answer(kTheCatalogueOfMunich, TheMunichCatalogue());
            chartVersions.Answer(beside / "1.pdf", 1473008);
            chartVersions.Answer(beside / "2.pdf", 1486381);
        }

        [[nodiscard]] DocumentsOfAnAddon Indexed()
        {
            const FakeFilesystemProbe probe(fileSystem);
            const DocumentService service(probe, catalogueParser, chartVersions);

            return service.DocumentsOf(Named("addon"), FolderOf("addon"), {"EDDM"});
        }
    };

    void DocumentServiceTest::OnlyTheChartsOfAPageThatRepeatsHaveTheirVersionRead()
    {
        AnAirportOfRepeatedPages munich;
        static_cast<void>(munich.Indexed());

        const std::filesystem::path beside = FolderOf("addon") / "NavDataPro" / "EDDM";

        QCOMPARE(munich.chartVersions.asked.size(), std::size_t{2});
        QCOMPARE(munich.chartVersions.asked.front(), beside / "1.pdf");
        QCOMPARE(munich.chartVersions.asked.back(), beside / "2.pdf");
    }

    void DocumentServiceTest::TheInformationLineCarriesTheNewestRevisionOfEachPage()
    {
        AnAirportOfRepeatedPages munich;
        const DocumentsOfAnAddon documents = munich.Indexed();
        const ChartsOfAnAirport* airport = AirportNamed(documents, "EDDM");

        QVERIFY(airport != nullptr);
        QCOMPARE(airport->types.size(), std::size_t{1});
        QCOMPARE(airport->types.front().charts.size(), std::size_t{2});
        QCOMPARE(airport->types.front().charts.front().pages.size(), std::size_t{2});
        QCOMPARE(airport->types.front().charts.front().pages.front(), PathFromUtf8("NavDataPro/EDDM/2.pdf"));
        QCOMPARE(airport->types.front().charts.back().revision, ChartRevision::Previous);
        QCOMPARE(airport->types.front().charts.back().pages.front(), PathFromUtf8("NavDataPro/EDDM/1.pdf"));
    }

    struct TwoAirportsOfRepeatedPages
    {
        InMemoryFileSystem fileSystem;
        FakeChartCatalogueParser catalogueParser;
        FakeChartVersions chartVersions;
        std::vector<AddonToRead> addons = {ToRead("one"), ToRead("two")};
        std::vector<AirportsOfAnAddon> airports = {
            {.addon = Named("one"), .evidence = AirportEvidence::TheCodeWasRead, .codes = {"EDDM"}},
            {.addon = Named("two"), .evidence = AirportEvidence::TheCodeWasRead, .codes = {"EDDM"}}};

        TwoAirportsOfRepeatedPages()
        {
            catalogueParser.Answer(kTheCatalogueOfMunich, TheMunichCatalogue());

            for (const std::string& addon : {"one", "two"})
            {
                const std::filesystem::path beside = FolderOf(addon) / "NavDataPro" / "EDDM";

                for (const std::string& chart : {"1.pdf", "2.pdf", "3.pdf"})
                {
                    fileSystem.AddFile(beside / chart, 100);
                    fileSystem.SetLastWriteTime(beside / chart,
                                                std::chrono::system_clock::time_point{std::chrono::hours{1}});
                }

                fileSystem.AddFileWithContents(beside / "catalogue.json", kTheCatalogueOfMunich);
                chartVersions.Answer(beside / "1.pdf", 1473008);
                chartVersions.Answer(beside / "2.pdf", 1486381);
            }
        }

        [[nodiscard]] std::vector<DocumentsOfAnAddon> Sweep(const std::vector<DocumentsOfAnAddon>& before,
                                                            const std::vector<AirportsOfAnAddon>& codes)
        {
            const FakeFilesystemProbe probe(fileSystem);
            const DocumentService service(probe, catalogueParser, chartVersions);

            return service.IndexWhile(addons, codes, before, {});
        }

        [[nodiscard]] std::vector<DocumentsOfAnAddon> Sweep(const std::vector<DocumentsOfAnAddon>& before)
        {
            return Sweep(before, airports);
        }
    };

    void DocumentServiceTest::ASecondSweepOverTheSameTreeReadsNoCatalogueAndNoChartVersion()
    {
        TwoAirportsOfRepeatedPages munich;

        const std::vector<DocumentsOfAnAddon> first = munich.Sweep({});

        QCOMPARE(munich.catalogueParser.parsed.size(), std::size_t{2});
        QCOMPARE(munich.chartVersions.asked.size(), std::size_t{4});

        const std::vector<DocumentsOfAnAddon> second = munich.Sweep(first);

        QCOMPARE(munich.catalogueParser.parsed.size(), std::size_t{2});
        QCOMPARE(munich.chartVersions.asked.size(), std::size_t{4});
        QCOMPARE(second.size(), first.size());
        QVERIFY(!second.front().digest.empty());
        QCOMPARE(second.front().digest, first.front().digest);
        QCOMPARE(second.front().airports.size(), std::size_t{1});
        QCOMPARE(second.front().airports.front().types.front().charts.size(), std::size_t{2});
    }

    void DocumentServiceTest::ChangingTheSizeOfOnePdfReclassifiesThatAddonAndNoOtherOne()
    {
        TwoAirportsOfRepeatedPages munich;

        const std::vector<DocumentsOfAnAddon> first = munich.Sweep({});

        munich.fileSystem.AddFile(FolderOf("one") / "NavDataPro" / "EDDM" / "1.pdf", 101);

        const std::vector<DocumentsOfAnAddon> second = munich.Sweep(first);

        QCOMPARE(munich.catalogueParser.parsed.size(), std::size_t{3});
        QCOMPARE(munich.chartVersions.asked.size(), std::size_t{6});
        QVERIFY(second.front().digest != first.front().digest);
        QCOMPARE(second.back().digest, first.back().digest);
    }

    void DocumentServiceTest::ChangingTheWriteTimeOfOnePdfAloneReclassifiesThatAddonAndNoOtherOne()
    {
        TwoAirportsOfRepeatedPages munich;

        const std::vector<DocumentsOfAnAddon> first = munich.Sweep({});

        munich.fileSystem.SetLastWriteTime(FolderOf("two") / "NavDataPro" / "EDDM" / "3.pdf",
                                           std::chrono::system_clock::time_point{std::chrono::hours{2}});

        const std::vector<DocumentsOfAnAddon> second = munich.Sweep(first);

        QCOMPARE(munich.catalogueParser.parsed.size(), std::size_t{3});
        QCOMPARE(munich.chartVersions.asked.size(), std::size_t{6});
        QCOMPARE(second.front().digest, first.front().digest);
        QVERIFY(second.back().digest != first.back().digest);
    }

    void DocumentServiceTest::ChangingTheWriteTimeOfAFileThatIsNoPdfReclassifiesThatAddonAndNoOtherOne()
    {
        TwoAirportsOfRepeatedPages munich;

        for (const std::string& addon : {"one", "two"})
        {
            munich.fileSystem.AddFile(FolderOf(addon) / "texture" / "ground.dds", 100);
            munich.fileSystem.SetLastWriteTime(FolderOf(addon) / "texture" / "ground.dds",
                                               std::chrono::system_clock::time_point{std::chrono::hours{1}});
        }

        const std::vector<DocumentsOfAnAddon> first = munich.Sweep({});

        munich.fileSystem.SetLastWriteTime(FolderOf("one") / "texture" / "ground.dds",
                                           std::chrono::system_clock::time_point{std::chrono::hours{1}}
                                               + std::chrono::milliseconds{1});

        const std::vector<DocumentsOfAnAddon> second = munich.Sweep(first);

        QCOMPARE(munich.catalogueParser.parsed.size(), std::size_t{3});
        QCOMPARE(munich.chartVersions.asked.size(), std::size_t{6});
        QVERIFY(second.front().digest != first.front().digest);
        QCOMPARE(second.back().digest, first.back().digest);
    }

    void DocumentServiceTest::TheDigestIsTakenFromTheWalkAndAsksNoFileForItsWriteTime()
    {
        TwoAirportsOfRepeatedPages munich;

        const FakeFilesystemProbe probe(munich.fileSystem);
        const DocumentService service(probe, munich.catalogueParser, munich.chartVersions);

        static_cast<void>(service.IndexWhile(munich.addons, munich.airports, {}, {}));

        QCOMPARE(probe.lastWriteTimesAsked, std::size_t{0});
    }

    void DocumentServiceTest::ChangedAirportCodesReclassifyTheAddon()
    {
        TwoAirportsOfRepeatedPages munich;

        const std::vector<DocumentsOfAnAddon> first = munich.Sweep({});

        QCOMPARE(first.front().airports.size(), std::size_t{1});

        const std::vector<DocumentsOfAnAddon> second =
            munich.Sweep(first,
                         {{.addon = Named("one"), .evidence = AirportEvidence::TheCodeWasRead, .codes = {"EBBR"}},
                          {.addon = Named("two"), .evidence = AirportEvidence::TheCodeWasRead, .codes = {"EDDM"}}});

        QVERIFY(second.front().digest != first.front().digest);
        QVERIFY(second.front().airports.empty());
        QCOMPARE(second.front().documents.size(), std::size_t{3});
        QCOMPARE(second.back().airports.size(), std::size_t{1});
        QCOMPARE(munich.catalogueParser.parsed.size(), std::size_t{2});
    }

    void DocumentServiceTest::AnIndexKeptBeforeTheDigestExistedReusesNothing()
    {
        TwoAirportsOfRepeatedPages munich;

        std::vector<DocumentsOfAnAddon> kept = munich.Sweep({});

        for (DocumentsOfAnAddon& addon : kept)
        {
            addon.digest.clear();
        }

        static_cast<void>(munich.Sweep(kept));

        QCOMPARE(munich.catalogueParser.parsed.size(), std::size_t{4});
        QCOMPARE(munich.chartVersions.asked.size(), std::size_t{8});
    }

    void DocumentServiceTest::AnAddonThatMovedToAnotherFolderIsNotTakenForTheOneThatWasIndexed()
    {
        TwoAirportsOfRepeatedPages munich;

        std::vector<DocumentsOfAnAddon> kept = munich.Sweep({});

        kept.front().folder = FolderOf("somewhere-else");
        kept.front().documents.push_back(PathFromUtf8("ghost.pdf"));

        const std::vector<DocumentsOfAnAddon> second = munich.Sweep(kept);

        QCOMPARE(second.front().folder, FolderOf("one"));
        QVERIFY(second.front().documents.empty());
        QCOMPARE(munich.catalogueParser.parsed.size(), std::size_t{3});
    }

    void DocumentServiceTest::AnAddonTheProbeCouldNotWalkIsNeverReused()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("addon") / "Manual.pdf");

        FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const FakeChartVersions chartVersions;
        const DocumentService service(probe, catalogueParser, chartVersions);

        const std::vector<DocumentsOfAnAddon> first = service.IndexWhile({ToRead("addon")}, {}, {}, {});

        probe.RefuseToWalk(FolderOf("addon"));

        const std::vector<DocumentsOfAnAddon> second = service.IndexWhile({ToRead("addon")}, {}, first, {});

        QVERIFY(!second.front().itWasWalked);
        QVERIFY(second.front().documents.empty());
    }

    void DocumentServiceTest::TheSweepAnswersPerAddonItWasGiven()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("brussels") / "NavDataPro" / "EBBR" / "53117.pdf");
        fileSystem.AddFile(FolderOf("sound-mod") / "readme.pdf");

        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const FakeChartVersions chartVersions;
        const DocumentService service(probe, catalogueParser, chartVersions);

        const std::vector<AirportsOfAnAddon> airports = {
            {.addon = Named("brussels"), .evidence = AirportEvidence::TheCodeWasRead, .codes = {"EBBR"}}};

        const std::vector<DocumentsOfAnAddon> indexed =
            service.IndexWhile({ToRead("brussels"), ToRead("sound-mod")}, airports, {}, {});

        QCOMPARE(indexed.size(), std::size_t{2});
        QCOMPARE(indexed.front().airports.size(), std::size_t{1});
        QVERIFY(indexed.back().airports.empty());
        QCOMPARE(indexed.back().documents.size(), std::size_t{1});
    }

    void DocumentServiceTest::TheSweepReportsProgressAndStopsWhereItIsToldTo()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("one") / "a.pdf");
        fileSystem.AddFile(FolderOf("two") / "b.pdf");
        fileSystem.AddFile(FolderOf("three") / "c.pdf");

        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const FakeChartVersions chartVersions;
        const DocumentService service(probe, catalogueParser, chartVersions);

        std::vector<std::size_t> seen;
        const std::vector<DocumentsOfAnAddon> indexed = service.IndexWhile(
            {ToRead("one"), ToRead("two"), ToRead("three")}, {}, {},
            [&seen](const DocumentsOfAnAddon&, const std::size_t indexedSoFar, const std::size_t outOf)
            {
                seen.push_back(outOf);

                return indexedSoFar < 2;
            });

        QCOMPARE(indexed.size(), std::size_t{2});
        QCOMPARE(seen.size(), std::size_t{2});
        QCOMPARE(seen.front(), std::size_t{3});
    }

    void DocumentServiceTest::TheSweepHandsOverEachAddonAsItFinishesReadingIt()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("one") / "a.pdf");
        fileSystem.AddFile(FolderOf("two") / "b.pdf");

        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const FakeChartVersions chartVersions;
        const DocumentService service(probe, catalogueParser, chartVersions);

        std::vector<std::string> handedOver;
        std::vector<std::size_t> carried;

        const std::vector<DocumentsOfAnAddon> indexed =
            service.IndexWhile({ToRead("one"), ToRead("two")}, {}, {},
                               [&handedOver, &carried](const DocumentsOfAnAddon& addon, std::size_t, std::size_t)
                               {
                                   handedOver.push_back(addon.addon.folderName);
                                   carried.push_back(addon.documents.size());

                                   return true;
                               });

        QCOMPARE(indexed.size(), std::size_t{2});
        QCOMPARE(handedOver.size(), std::size_t{2});
        QCOMPARE(handedOver.front(), std::string{"one"});
        QCOMPARE(handedOver.back(), std::string{"two"});
        QVERIFY2(carried.front() == std::size_t{1},
                 "the addon is handed over already read, because a name with nothing behind it puts no line on the "
                 "screen");
    }

    void DocumentServiceTest::AnAddonTheProbeCannotWalkSaysSoInsteadOfSayingItHasNothing()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("addon") / "Manual.pdf");

        FakeFilesystemProbe probe(fileSystem);
        probe.RefuseToWalk(FolderOf("addon"));

        const FakeChartCatalogueParser catalogueParser;
        const FakeChartVersions chartVersions;
        const DocumentService service(probe, catalogueParser, chartVersions);

        const DocumentsOfAnAddon documents = service.DocumentsOf(Named("addon"), FolderOf("addon"), {});

        QVERIFY(!documents.itWasWalked);
        QVERIFY(documents.documents.empty());
    }
}

QTEST_APPLESS_MAIN(DocumentServiceTest)

#include "tst_document_service.moc"
