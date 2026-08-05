#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include "domain/model/CategoryMarker.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "tests/support/DeepPaths.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class CatalogOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnAddonPastTheOldCeilingIsScannedWithTheTitleFromItsManifest();
        static void ACategoryMarkerPastTheOldCeilingIsRead();
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

    struct Catalog
    {
        JsonManifestParser manifestParser;
        WindowsFilesystemProbe filesystemProbe;
        FilesystemScanner scanner{manifestParser, filesystemProbe};
    };

    [[nodiscard]] const TreeNode* Only(const TreeNode& parent)
    {
        return parent.children.size() == 1 ? &parent.children.front() : nullptr;
    }
}

void CatalogOnRealDiskTest::AnAddonPastTheOldCeilingIsScannedWithTheTitleFromItsManifest()
{
    const Disk disk;
    const std::filesystem::path library = FolderPastTheCeiling(disk.Root(), "Library");
    const std::filesystem::path addon = library / "Utils" / "tfdidesign-aircraft-md11";
    QVERIFY(addon.wstring().size() > kOldPathCeiling);

    WriteFilePastTheCeiling(ManifestPathIn(addon), R"({"title": "MD-11", "package_version": "0.6.3"})");

    const Catalog catalog;
    const TreeNode root = catalog.scanner.Scan(library);

    QCOMPARE(root.kind, TreeNodeKind::Library);

    const TreeNode* category = Only(root);
    QVERIFY(category != nullptr);
    QCOMPARE(category->kind, TreeNodeKind::Category);

    const TreeNode* scanned = Only(*category);
    QVERIFY(scanned != nullptr);
    QCOMPARE(scanned->kind, TreeNodeKind::Addon);
    QCOMPARE(scanned->path, addon);

    QVERIFY(scanned->addon.has_value());
    QCOMPARE(scanned->addon->manifest.title, std::string{"MD-11"});
    QCOMPARE(scanned->addon->manifest.packageVersion, std::string{"0.6.3"});
}

void CatalogOnRealDiskTest::ACategoryMarkerPastTheOldCeilingIsRead()
{
    const Disk disk;
    const std::filesystem::path library = FolderPastTheCeiling(disk.Root(), "Library");
    const std::filesystem::path declared = library / "Sceneries";
    const std::filesystem::path plain = library / "Utils";

    WriteFilePastTheCeiling(CategoryMarkerPathIn(declared), "");
    std::filesystem::create_directories(BeyondTheCeiling(plain));

    const Catalog catalog;
    const TreeNode root = catalog.scanner.Scan(library);

    QCOMPARE(root.children.size(), std::size_t{2});

    for (const TreeNode& child : root.children)
    {
        QCOMPARE(child.kind, TreeNodeKind::Category);
        QCOMPARE(child.declaredAsCategory, child.path == declared);
    }
}

QTEST_APPLESS_MAIN(CatalogOnRealDiskTest)

#include "tst_catalog_on_real_disk.moc"
