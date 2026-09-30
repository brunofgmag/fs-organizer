#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <chrono>
#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

#include "application/DocumentService.h"
#include "domain/support/PathUtils.h"
#include "domain/ports/ImportedFolders.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonChartCatalogueParser.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/documents/QtPdfChartVersions.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "tests/support/APdf.h"
#include "tests/support/PathPrinting.h"

namespace
{
    const NothingWasImported nothingWasImported;

    class DocumentsOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheChartsOfARealAddonComeOutNamedByTheCatalogueThatSitsBesideThem();
        static void TheSweepReachesEveryAddonTheRealScanFinds();
        static void TheOlderRevisionOfAnInformationPageComesOutOnASecondLineReadFromTheRealPdfs();
        static void RewritingTheWriteTimeOfOneFileOnDiskReindexesThatAddonAndNoOtherOne();
        static void AnUntouchedTreeKeepsItsDigestAndOneMillisecondOnAnyFileChangesIt();
        static void TheAddonsTheSweepReadsCarryTheIdentityTheScanUsedToGiveThem();
    };
}

namespace
{
    struct Disk
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return directory.path().toStdWString();
        }
    };

    struct Indexing
    {
        JsonManifestParser manifestParser;
        JsonChartCatalogueParser catalogueParser;
        WindowsFilesystemProbe filesystemProbe;
        FilesystemScanner scanner{manifestParser, filesystemProbe, nothingWasImported};
        QtPdfChartVersions chartVersions;
        DocumentService service{filesystemProbe, catalogueParser, chartVersions};

        [[nodiscard]] std::vector<AddonToRead> AddonsOf(const std::filesystem::path& library) const
        {
            SimulatorProfile profile;
            profile.id = "msfs2024";
            profile.libraries = {{.id = "library-1", .path = library, .label = "Sceneries"}};

            return SceneryService::AddonsOf(profile, {.libraries = {scanner.Scan(library)}});
        }
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
    const std::vector<DocumentsOfAnAddon> indexed = indexing.service.IndexWhile(indexing.AddonsOf(library), {}, {}, {});

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

void DocumentsOnRealDiskTest::RewritingTheWriteTimeOfOneFileOnDiskReindexesThatAddonAndNoOtherOne()
{
    const Disk disk;
    const std::filesystem::path library = disk.Root() / "Sceneries";

    WriteFile(library / "brussels" / "manifest.json", R"({"title": "Brussels"})");
    WriteFile(library / "brussels" / "readme.pdf", "%PDF-1.4 a readme");
    WriteFile(library / "sound-mod" / "manifest.json", R"({"title": "Sound"})");
    WriteFile(library / "sound-mod" / "readme.pdf", "%PDF-1.4 a readme");

    const Indexing indexing;
    const std::vector<AddonToRead> addons = indexing.AddonsOf(library);

    std::vector<DocumentsOfAnAddon> before = indexing.service.IndexWhile(addons, {}, {}, {});

    QCOMPARE(before.size(), std::size_t{2});

    for (DocumentsOfAnAddon& addon : before)
    {
        QVERIFY(!addon.digest.empty());
        QCOMPARE(addon.documents.size(), std::size_t{1});

        addon.documents.clear();
    }

    const std::vector<DocumentsOfAnAddon> untouched = indexing.service.IndexWhile(addons, {}, before, {});

    QCOMPARE(untouched.front().documents.size(), std::size_t{0});
    QCOMPARE(untouched.back().documents.size(), std::size_t{0});

    for (int again = 0; again < 20; ++again)
    {
        const std::vector<DocumentsOfAnAddon> repeated = indexing.service.IndexWhile(addons, {}, {}, {});

        QCOMPARE(repeated.front().digest, before.front().digest);
        QCOMPARE(repeated.back().digest, before.back().digest);
    }

    std::filesystem::last_write_time(library / "brussels" / "readme.pdf",
                                     std::filesystem::file_time_type::clock::now() - std::chrono::hours(5));

    const std::vector<DocumentsOfAnAddon> rewritten = indexing.service.IndexWhile(addons, {}, before, {});

    QCOMPARE(rewritten.front().addon.folderName, std::string{"brussels"});
    QCOMPARE(rewritten.front().documents.size(), std::size_t{1});
    QVERIFY(rewritten.front().digest != before.front().digest);
    QCOMPARE(rewritten.back().documents.size(), std::size_t{0});
    QCOMPARE(rewritten.back().digest, before.back().digest);
}

void DocumentsOnRealDiskTest::AnUntouchedTreeKeepsItsDigestAndOneMillisecondOnAnyFileChangesIt()
{
    const Disk disk;
    const std::filesystem::path library = disk.Root() / "Sceneries";
    const std::filesystem::path texture = library / "brussels" / "texture" / "ground.dds";

    WriteFile(library / "brussels" / "manifest.json", R"({"title": "Brussels"})");
    WriteFile(library / "brussels" / "readme.pdf", "%PDF-1.4 a readme");
    WriteFile(texture, "a texture");
    WriteFile(library / "sound-mod" / "manifest.json", R"({"title": "Sound"})");
    WriteFile(library / "sound-mod" / "readme.pdf", "%PDF-1.4 a readme");

    const Indexing indexing;
    const std::vector<AddonToRead> addons = indexing.AddonsOf(library);

    const std::vector<DocumentsOfAnAddon> before = indexing.service.IndexWhile(addons, {}, {}, {});

    QCOMPARE(before.size(), std::size_t{2});

    for (int again = 0; again < 20; ++again)
    {
        const std::vector<DocumentsOfAnAddon> repeated = indexing.service.IndexWhile(addons, {}, {}, {});

        QCOMPARE(repeated.front().digest, before.front().digest);
        QCOMPARE(repeated.back().digest, before.back().digest);
    }

    std::filesystem::last_write_time(texture, std::filesystem::last_write_time(texture) + std::chrono::milliseconds(1));

    const std::vector<DocumentsOfAnAddon> moved = indexing.service.IndexWhile(addons, {}, {}, {});

    QVERIFY(moved.front().digest != before.front().digest);
    QCOMPARE(moved.back().digest, before.back().digest);
}

void DocumentsOnRealDiskTest::TheAddonsTheSweepReadsCarryTheIdentityTheScanUsedToGiveThem()
{
    const Disk disk;
    const std::filesystem::path library = disk.Root() / "Sceneries";

    WriteFile(library / "brussels" / "manifest.json", R"({"title": "Brussels"})");
    WriteFile(library / "brussels" / "readme.pdf", "%PDF-1.4 a readme");

    const Indexing indexing;
    const std::vector<AddonToRead> addons = indexing.AddonsOf(library);

    QCOMPARE(addons.size(), std::size_t{1});
    QCOMPARE(addons.front().addon.libraryId, LibraryId{"library-1"});
    QCOMPARE(addons.front().addon.folderName, std::string{"brussels"});
    QCOMPARE(addons.front().folder, library / "brussels");
}

QTEST_APPLESS_MAIN(DocumentsOnRealDiskTest)

#include "tst_documents_on_real_disk.moc"
