#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/StdFilesystemProbe.h"

class FilesystemScannerTest : public QObject
{
    Q_OBJECT

private slots:
    static void ScanningStopsAtTheFirstManifest();
    static void AnAddonCarriesTheMetadataFromItsManifest();
    static void AnEmptyFolderIsAnEmptyCategory();
    static void AFolderWithAnUnreadableManifestIsStillAnAddon();
    static void WhatTheImporterCreatedIsNotPartOfTheLibrary();
};

namespace
{
    struct Library
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return {directory.path().toStdString()};
        }

        void AddFolder(const std::string& relativePath) const
        {
            std::filesystem::create_directories(Root() / relativePath);
        }

        void AddManifest(const std::string& relativePath, const std::string& content) const
        {
            AddFolder(relativePath);
            std::ofstream file(Root() / relativePath / "manifest.json", std::ios::binary);
            file << content;
        }
    };

    const JsonManifestParser parser;
    const StdFilesystemProbe probe;

    const TreeNode* ChildNamed(const TreeNode& parent, const std::string& name)
    {
        const auto child = std::ranges::find_if(parent.children, [&name](const TreeNode& node)
        {
            return node.path.filename() == name;
        });

        return child == parent.children.end() ? nullptr : &*child;
    }
}

void FilesystemScannerTest::ScanningStopsAtTheFirstManifest()
{
    const Library library;
    library.AddManifest("Aircraft Mods/pmdg-aircraft-77w", R"({"title": "PMDG 777"})");
    library.AddManifest("Aircraft Mods/pmdg-aircraft-77w/SimObjects/inner", R"({"title": "Inner"})");

    const FilesystemScanner scanner(parser, probe);
    const TreeNode root = scanner.Scan(library.Root());

    QCOMPARE(root.kind, TreeNodeKind::Library);
    QCOMPARE(root.children.size(), std::size_t{1});

    const TreeNode& category = root.children.front();
    QCOMPARE(category.kind, TreeNodeKind::Category);
    QCOMPARE(category.path.filename(), std::filesystem::path("Aircraft Mods"));
    QCOMPARE(category.children.size(), std::size_t{1});

    const TreeNode& addon = category.children.front();
    QCOMPARE(addon.kind, TreeNodeKind::Addon);
    QCOMPARE(addon.path.filename(), std::filesystem::path("pmdg-aircraft-77w"));
    QCOMPARE(addon.children.size(), std::size_t{0});
}

void FilesystemScannerTest::AnAddonCarriesTheMetadataFromItsManifest()
{
    const Library library;
    library.AddManifest("Sceneries/tlc-bgjn", R"({
      "content_type": "SCENERY",
      "title": "Ilulissat",
      "creator": "TLC",
      "package_version": "1.2.0"
    })");

    const FilesystemScanner scanner(parser, probe);
    const TreeNode root = scanner.Scan(library.Root());
    const TreeNode& addon = root.children.front().children.front();

    QVERIFY(addon.addon.has_value());
    QCOMPARE(addon.addon->folderPath, library.Root() / "Sceneries" / "tlc-bgjn");
    QCOMPARE(addon.addon->manifest.title, std::string("Ilulissat"));
    QCOMPARE(addon.addon->manifest.creator, std::string("TLC"));
    QCOMPARE(addon.addon->manifest.contentType, std::string("SCENERY"));
    QCOMPARE(addon.addon->manifest.packageVersion, std::string("1.2.0"));
}

void FilesystemScannerTest::AnEmptyFolderIsAnEmptyCategory()
{
    const Library library;
    library.AddFolder("Liveries");
    library.AddManifest("Sceneries/tlc-bgjn", R"({"title": "Ilulissat"})");

    const FilesystemScanner scanner(parser, probe);
    const TreeNode root = scanner.Scan(library.Root());

    QCOMPARE(root.children.size(), std::size_t{2});

    const TreeNode* liveries = ChildNamed(root, "Liveries");
    QVERIFY(liveries != nullptr);
    QCOMPARE(liveries->kind, TreeNodeKind::Category);
    QCOMPARE(liveries->children.size(), std::size_t{0});
    QVERIFY(!liveries->addon.has_value());
}

void FilesystemScannerTest::AFolderWithAnUnreadableManifestIsStillAnAddon()
{
    const Library library;
    library.AddManifest("Sceneries/half-written-addon", "{\"title\":");

    const FilesystemScanner scanner(parser, probe);
    const TreeNode root = scanner.Scan(library.Root());

    const TreeNode* sceneries = ChildNamed(root, "Sceneries");
    QVERIFY(sceneries != nullptr);

    const TreeNode* addon = ChildNamed(*sceneries, "half-written-addon");
    QVERIFY(addon != nullptr);
    QCOMPARE(addon->kind, TreeNodeKind::Addon);
    QVERIFY(addon->addon.has_value());
    QCOMPARE(addon->addon->manifest.title, std::string());
}

void FilesystemScannerTest::WhatTheImporterCreatedIsNotPartOfTheLibrary()
{
    const Library library;
    library.AddManifest("Sceneries/tlc-bgjn", R"({"title": "Ilulissat"})");
    library.AddManifest("_fsorganizer-quarantine/tlc-bgjn", R"({"title": "Ilulissat antigo"})");
    library.AddManifest("Sceneries/fss-aircraft-727.fsorg-partial", R"({"title": "Meia importacao"})");

    const FilesystemScanner scanner(parser, probe);
    const TreeNode root = scanner.Scan(library.Root());

    QCOMPARE(root.children.size(), std::size_t{1});
    QVERIFY(ChildNamed(root, "_fsorganizer-quarantine") == nullptr);

    const TreeNode* sceneries = ChildNamed(root, "Sceneries");
    QVERIFY(sceneries != nullptr);
    QCOMPARE(sceneries->children.size(), std::size_t{1});
    QVERIFY(ChildNamed(*sceneries, "fss-aircraft-727.fsorg-partial") == nullptr);
}

QTEST_APPLESS_MAIN(FilesystemScannerTest)

#include "tst_filesystem_scanner.moc"
