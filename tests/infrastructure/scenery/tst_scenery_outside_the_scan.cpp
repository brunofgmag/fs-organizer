#include <QtTest/QtTest>

#include "domain/support/PathUtils.h"
#include "domain/ports/ImportedFolders.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/InMemoryFileSystem.h"

namespace
{
    const NothingWasImported nothingWasImported;

    class SceneryOutsideTheScanTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheScanOpensNoSceneryFile();
    };

    const std::filesystem::path kLibrary = PathFromUtf8("D:/Library");
    const std::filesystem::path kAddon = PathFromUtf8("D:/Library/Sceneries/someone-airport-eham");
}

void SceneryOutsideTheScanTest::TheScanOpensNoSceneryFile()
{
    InMemoryFileSystem fileSystem;
    fileSystem.AddDirectory(kLibrary);
    fileSystem.AddFileWithContents(kAddon / "manifest.json", R"({"title":"EHAM","content_type":"SCENERY"})");
    fileSystem.AddFile(kAddon / "scenery" / "APX00000.bgl", 4096);
    fileSystem.AddFile(kAddon / "scenery" / "APX00001.bgl", 4096);

    const JsonManifestParser manifestParser;
    const FakeFilesystemProbe filesystemProbe(fileSystem);
    const FilesystemScanner scanner(manifestParser, filesystemProbe, nothingWasImported);

    const TreeNode library = scanner.Scan(kLibrary);

    QCOMPARE(filesystemProbe.TimesItReadSomethingEndingIn(".bgl"), std::size_t{0});
    QVERIFY2(filesystemProbe.TimesItReadSomethingEndingIn("manifest.json") > 0,
             "the count above is only worth reading if the scan reads anything at all through this probe");
    QVERIFY(!library.children.empty());
}

QTEST_APPLESS_MAIN(SceneryOutsideTheScanTest)

#include "tst_scenery_outside_the_scan.moc"
