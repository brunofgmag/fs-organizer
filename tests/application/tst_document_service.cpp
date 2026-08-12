#include <QtTest/QtTest>

#include <cstddef>
#include <string>
#include <vector>

#include "application/DocumentService.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeChartCatalogueParser.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/InMemoryFileSystem.h"
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
        static void TheSweepWalksEveryLibraryThroughTheScanAndAnswersPerAddon();
        static void TheSweepReportsProgressAndStopsWhereItIsToldTo();
        static void AnAddonTheProbeCannotWalkSaysSoInsteadOfSayingItHasNothing();
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

    [[nodiscard]] TreeNode AddonNode(const std::string& folderName)
    {
        return {.kind = TreeNodeKind::Addon, .path = FolderOf(folderName), .addon = Addon{}, .children = {}};
    }

    [[nodiscard]] TreeNode LibraryNode(std::vector<TreeNode> addons)
    {
        return {.kind = TreeNodeKind::Library, .path = kLibrary, .addon = {}, .children = std::move(addons)};
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

        const FakeCatalogScanner scanner;
        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const DocumentService service(scanner, probe, catalogueParser);

        const DocumentsOfAnAddon documents = service.DocumentsOf(Named("addon"), FolderOf("addon"), {});

        QCOMPARE(documents.documents.size(), std::size_t{1});
        QCOMPARE(documents.documents.front(), PathFromUtf8("Manual.pdf"));
    }

    void DocumentServiceTest::APdfUnderAFolderNamedAfterACodeTheAddonCarriesIsACharted()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("addon") / "Manual.pdf");
        fileSystem.AddFile(FolderOf("addon") / "NavDataPro" / "EBBR" / "53117.pdf");

        const FakeCatalogScanner scanner;
        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const DocumentService service(scanner, probe, catalogueParser);

        const DocumentsOfAnAddon documents = service.DocumentsOf(Named("addon"), FolderOf("addon"), {"EBBR"});

        QCOMPARE(documents.documents.size(), std::size_t{1});
        QCOMPARE(documents.airports.size(), std::size_t{1});
        QCOMPARE(QString::fromStdString(documents.airports.front().code), QString("EBBR"));
    }

    void DocumentServiceTest::AnAddonWhoseCodeWasNeverExtractedProducesNoChartByCode()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("addon") / "NavDataPro" / "EBBR" / "53117.pdf");

        const FakeCatalogScanner scanner;
        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const DocumentService service(scanner, probe, catalogueParser);

        const DocumentsOfAnAddon documents = service.DocumentsOf(Named("addon"), FolderOf("addon"), {});

        QVERIFY(documents.airports.empty());
        QCOMPARE(documents.documents.size(), std::size_t{1});
    }

    void DocumentServiceTest::AFolderNamedDocsNeverBecomesAnAirport()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("addon") / "DOCS" / "handbook.pdf");

        const FakeCatalogScanner scanner;
        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const DocumentService service(scanner, probe, catalogueParser);

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

        const FakeCatalogScanner scanner;
        const FakeFilesystemProbe probe(fileSystem);
        FakeChartCatalogueParser catalogueParser;
        catalogueParser.Answer(kTheCatalogueOfBrussels, TheBrusselsCatalogue());

        const DocumentService service(scanner, probe, catalogueParser);

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

        const FakeCatalogScanner scanner;
        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const DocumentService service(scanner, probe, catalogueParser);

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

        const FakeCatalogScanner scanner;
        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const DocumentService service(scanner, probe, catalogueParser);

        const DocumentsOfAnAddon documents = service.DocumentsOf(Named("addon"), FolderOf("addon"), {"EBBR"});

        QVERIFY(documents.documents.empty());
        QVERIFY(documents.airports.empty());
        QVERIFY(documents.itWasWalked);
    }

    void DocumentServiceTest::TheSweepWalksEveryLibraryThroughTheScanAndAnswersPerAddon()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("brussels") / "NavDataPro" / "EBBR" / "53117.pdf");
        fileSystem.AddFile(FolderOf("sound-mod") / "readme.pdf");

        FakeCatalogScanner scanner;
        scanner.SetTree(kLibrary, LibraryNode({AddonNode("brussels"), AddonNode("sound-mod")}));

        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const DocumentService service(scanner, probe, catalogueParser);

        const std::vector<AirportsOfAnAddon> airports = {
            {.addon = Named("brussels"), .evidence = AirportEvidence::TheCodeWasRead, .codes = {"EBBR"}}};

        const std::vector<DocumentsOfAnAddon> indexed =
            service.IndexWhile({{.id = kLibraryId, .path = kLibrary, .label = "Sceneries"}}, airports, {});

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

        FakeCatalogScanner scanner;
        scanner.SetTree(kLibrary, LibraryNode({AddonNode("one"), AddonNode("two"), AddonNode("three")}));

        const FakeFilesystemProbe probe(fileSystem);
        const FakeChartCatalogueParser catalogueParser;
        const DocumentService service(scanner, probe, catalogueParser);

        std::vector<std::size_t> seen;
        const std::vector<DocumentsOfAnAddon> indexed =
            service.IndexWhile({{.id = kLibraryId, .path = kLibrary, .label = "Sceneries"}}, {},
                               [&seen](const std::size_t indexedSoFar, const std::size_t outOf)
                               {
                                   seen.push_back(outOf);

                                   return indexedSoFar < 2;
                               });

        QCOMPARE(indexed.size(), std::size_t{2});
        QCOMPARE(seen.size(), std::size_t{2});
        QCOMPARE(seen.front(), std::size_t{3});
    }

    void DocumentServiceTest::AnAddonTheProbeCannotWalkSaysSoInsteadOfSayingItHasNothing()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddFile(FolderOf("addon") / "Manual.pdf");

        const FakeCatalogScanner scanner;
        FakeFilesystemProbe probe(fileSystem);
        probe.RefuseToWalk(FolderOf("addon"));

        const FakeChartCatalogueParser catalogueParser;
        const DocumentService service(scanner, probe, catalogueParser);

        const DocumentsOfAnAddon documents = service.DocumentsOf(Named("addon"), FolderOf("addon"), {});

        QVERIFY(!documents.itWasWalked);
        QVERIFY(documents.documents.empty());
    }
}

QTEST_APPLESS_MAIN(DocumentServiceTest)

#include "tst_document_service.moc"
