#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <cstddef>
#include <fstream>
#include <string>

#include "application/DocumentService.h"
#include "domain/support/PathUtils.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonChartCatalogueParser.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/documents/QtPdfChartVersions.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "tests/support/APdf.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class DocumentsOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheChartsOfARealAddonComeOutNamedByTheCatalogueThatSitsBesideThem();
        static void TheSweepReachesEveryAddonTheRealScanFinds();
        static void TheOlderRevisionOfAnInformationPageComesOutOnASecondLineReadFromTheRealPdfs();
    };
}

namespace
{
    struct Disk
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return std::filesystem::path(directory.path().toStdWString());
        }
    };

    struct Indexing
    {
        JsonManifestParser manifestParser;
        JsonChartCatalogueParser catalogueParser;
        WindowsFilesystemProbe filesystemProbe;
        FilesystemScanner scanner{manifestParser, filesystemProbe};
        QtPdfChartVersions chartVersions;
        DocumentService service{scanner, filesystemProbe, catalogueParser, chartVersions};
    };

    void WriteFile(const std::filesystem::path& file, const std::string& text)
    {
        std::filesystem::create_directories(file.parent_path());
        std::ofstream stream(file, std::ios::binary);
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
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
}

void DocumentsOnRealDiskTest::TheChartsOfARealAddonComeOutNamedByTheCatalogueThatSitsBesideThem()
{
    const Disk disk;
    const std::filesystem::path addon = disk.Root() / "Sceneries" / "aerosoft-airport-ebbr-brussels";

    WriteFile(addon / "Manual_MegaAirport.pdf", "%PDF-1.4 a manual");
    WriteFile(addon / "NavDataPro" / "EBBR" / "53117.pdf", "%PDF-1.4 a chart");
    WriteFile(addon / "NavDataPro" / "EBBR" / "53206.pdf", "%PDF-1.4 another chart");
    WriteFile(addon / "NavDataPro" / "EBBR" / "catalogue.json",
              R"({"icao":"EBBR","catalogue":[
                   {"chart_id":"53117","chart_type":"AFC","chart_name":"AFC"},
                   {"chart_id":"53206","chart_type":"IAC","chart_name":"ILS or LOC Y 25L"}]})");

    const Indexing indexing;
    const DocumentsOfAnAddon documents = indexing.service.DocumentsOf(
        {.libraryId = "library-1", .folderName = "aerosoft-airport-ebbr-brussels"}, addon, {"EBBR"});

    QVERIFY(documents.itWasWalked);
    QCOMPARE(documents.documents.size(), std::size_t{1});
    QCOMPARE(documents.documents.front(), PathFromUtf8("Manual_MegaAirport.pdf"));

    const ChartsOfAnAirport* brussels = AirportNamed(documents, "EBBR");

    QVERIFY(brussels != nullptr);
    QVERIFY(brussels->catalogued);
    QCOMPARE(brussels->types.size(), std::size_t{2});
    QCOMPARE(QString::fromStdString(brussels->types.back().charts.front().name), QString("ILS or LOC Y 25L"));
    QCOMPARE(brussels->types.back().charts.front().pages.front(), PathFromUtf8("NavDataPro/EBBR/53206.pdf"));
}

void DocumentsOnRealDiskTest::TheSweepReachesEveryAddonTheRealScanFinds()
{
    const Disk disk;
    const std::filesystem::path library = disk.Root() / "Sceneries";

    WriteFile(library / "brussels" / "manifest.json", R"({"title": "Brussels"})");
    WriteFile(library / "brussels" / "Charts" / "approach.pdf", "%PDF-1.4 a chart");
    WriteFile(library / "sound-mod" / "manifest.json", R"({"title": "Sound"})");
    WriteFile(library / "sound-mod" / "readme.pdf", "%PDF-1.4 a readme");

    const Indexing indexing;
    const std::vector<DocumentsOfAnAddon> indexed =
        indexing.service.IndexWhile({{.id = "library-1", .path = library, .label = "Sceneries"}}, {}, {});

    QCOMPARE(indexed.size(), std::size_t{2});

    const ChartsOfAnAirport* undetermined = AirportNamed(indexed.front(), {});

    QVERIFY(undetermined != nullptr);
    QVERIFY(!undetermined->catalogued);
    QCOMPARE(indexed.back().documents.size(), std::size_t{1});
}

void DocumentsOnRealDiskTest::TheOlderRevisionOfAnInformationPageComesOutOnASecondLineReadFromTheRealPdfs()
{
    const Disk disk;
    const std::filesystem::path addon = disk.Root() / "Sceneries" / "aerosoft-airport-eddm-munich";
    const std::filesystem::path beside = addon / "NavDataPro" / "EDDM";

    WriteFile(beside / "60001.pdf", AChartOfVersion(1473008));
    WriteFile(beside / "60002.pdf", AChartOfVersion(1486381));
    WriteFile(beside / "60003.pdf", AChartOfVersion(1486382));
    WriteFile(beside / "catalogue.json",
              R"({"icao":"EDDM","catalogue":[
                   {"chart_id":"60001","chart_type":"AOI","chart_name":"1"},
                   {"chart_id":"60002","chart_type":"AOI","chart_name":"1"},
                   {"chart_id":"60003","chart_type":"AOI","chart_name":"2"}]})");

    const Indexing indexing;
    const DocumentsOfAnAddon documents = indexing.service.DocumentsOf(
        {.libraryId = "library-1", .folderName = "aerosoft-airport-eddm-munich"}, addon, {"EDDM"});

    const ChartsOfAnAirport* munich = AirportNamed(documents, "EDDM");

    QVERIFY(munich != nullptr);
    QCOMPARE(munich->types.size(), std::size_t{1});
    QCOMPARE(munich->types.front().charts.size(), std::size_t{2});
    QCOMPARE(munich->types.front().charts.front().pages.size(), std::size_t{2});
    QCOMPARE(munich->types.front().charts.front().pages.front(), PathFromUtf8("NavDataPro/EDDM/60002.pdf"));
    QCOMPARE(munich->types.front().charts.back().pages.size(), std::size_t{1});
    QCOMPARE(munich->types.front().charts.back().pages.front(), PathFromUtf8("NavDataPro/EDDM/60001.pdf"));
}

QTEST_APPLESS_MAIN(DocumentsOnRealDiskTest)

#include "tst_documents_on_real_disk.moc"
