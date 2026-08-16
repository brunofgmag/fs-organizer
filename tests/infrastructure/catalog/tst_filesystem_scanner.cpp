#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include "domain/model/CategoryMarker.h"
#include "domain/tree/AddonTree.h"
#include "domain/ports/ImportedFolders.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/StdFilesystemProbe.h"

namespace
{
    const NothingWasImported nothingWasImported;

    class FilesystemScannerTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ScanningStopsAtTheFirstManifest();
        static void AnAddonCarriesTheMetadataFromItsManifest();
        static void AFolderWithoutAManifestThatGroupsNoAddonIsItselfAnAddon();
        static void AFolderThatStillGroupsAnAddonStaysACategory();
        static void WhatTheImporterBroughtInIsAnAddonEvenWhenItHoldsOne();
        static void AnEmptyFolderIsAnAddonUnlessTheMarkerDeclaresIt();
        static void ACategoryTheAppDeclaredSurvivesLosingItsLastAddon();
        static void EveryCategoryTheScanReturnsHoldsAddonsOrWasDeclared();
        static void AFolderWithAnUnreadableManifestIsStillAnAddon();
        static void WhatTheImporterCreatedIsNotPartOfTheLibrary();
        static void AGateThatClosesStopsTheWalkWhereItWasInsteadOfFinishing();
    };
}

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

        void Declare(const std::string& relativePath) const
        {
            AddFolder(relativePath);
            const std::ofstream marker(CategoryMarkerPathIn(Root() / relativePath), std::ios::binary);
        }
    };

    const JsonManifestParser parser;
    const StdFilesystemProbe probe;

    const TreeNode* ChildNamed(const TreeNode& parent, const std::string& name)
    {
        const auto child = std::ranges::find_if(parent.children,
                                                [&name](const TreeNode& node)
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

    const FilesystemScanner scanner(parser, probe, nothingWasImported);
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

void FilesystemScannerTest::AGateThatClosesStopsTheWalkWhereItWasInsteadOfFinishing()
{
    const Library library;
    library.AddManifest("Aircraft Mods/pmdg-aircraft-77w", R"({"title": "PMDG 777"})");
    library.AddManifest("Sceneries/tlc-bgjn", R"({"title": "Ilulissat"})");
    library.AddManifest("Liveries/fenix-a320", R"({"title": "Fenix"})");

    const FilesystemScanner scanner(parser, probe, nothingWasImported);

    int asked = 0;
    const ScanGate closesAfterTheFirstFolder{.keepGoing = [&asked]
                                             {
                                                 return ++asked <= 1;
                                             }};

    const TreeNode stopped = scanner.ScanWhile(library.Root(), closesAfterTheFirstFolder);
    const TreeNode whole = scanner.Scan(library.Root());

    QCOMPARE(whole.children.size(), std::size_t{3});
    QCOMPARE(stopped.children.size(), std::size_t{1});
    QVERIFY2(stopped.children.front().children.empty(), "the walk kept descending after the gate had closed");
    QCOMPARE(stopped.children.front().kind, TreeNodeKind::Category);
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

    const FilesystemScanner scanner(parser, probe, nothingWasImported);
    const TreeNode root = scanner.Scan(library.Root());
    const TreeNode& addon = root.children.front().children.front();

    QVERIFY(addon.addon.has_value());
    QCOMPARE(addon.addon->folderPath, library.Root() / "Sceneries" / "tlc-bgjn");
    QCOMPARE(addon.addon->manifest.title, std::string("Ilulissat"));
    QCOMPARE(addon.addon->manifest.creator, std::string("TLC"));
    QCOMPARE(addon.addon->manifest.contentType, std::string("SCENERY"));
    QCOMPARE(addon.addon->manifest.packageVersion, std::string("1.2.0"));
}

void FilesystemScannerTest::AFolderWithoutAManifestThatGroupsNoAddonIsItselfAnAddon()
{
    const Library library;
    library.AddManifest("Aircrafts/fsl-a32x", R"({"title": "A32X"})");
    library.AddFolder("Aircrafts/fsl-a32x_CVT_/EFFECTS/TEXTURE");

    const FilesystemScanner scanner(parser, probe, nothingWasImported);
    const TreeNode root = scanner.Scan(library.Root());

    const TreeNode* aircrafts = ChildNamed(root, "Aircrafts");
    QVERIFY(aircrafts != nullptr);
    QCOMPARE(aircrafts->kind, TreeNodeKind::Category);
    QCOMPARE(aircrafts->children.size(), std::size_t{2});

    const TreeNode* converted = ChildNamed(*aircrafts, "fsl-a32x_CVT_");
    QVERIFY(converted != nullptr);
    QCOMPARE(converted->kind, TreeNodeKind::Addon);
    QVERIFY(converted->addon.has_value());
    QCOMPARE(converted->addon->folderPath, library.Root() / "Aircrafts" / "fsl-a32x_CVT_");
    QVERIFY2(converted->children.empty(), "the walk kept exposing what is inside a folder it had already adopted");
}

void FilesystemScannerTest::AFolderThatStillGroupsAnAddonStaysACategory()
{
    const Library library;
    library.AddManifest("Utils/flybywire-current-install-804/restore", R"({"title": "A380X"})");

    const FilesystemScanner scanner(parser, probe, nothingWasImported);
    const TreeNode root = scanner.Scan(library.Root());

    const TreeNode* utils = ChildNamed(root, "Utils");
    QVERIFY(utils != nullptr);

    const TreeNode* installer = ChildNamed(*utils, "flybywire-current-install-804");
    QVERIFY(installer != nullptr);
    QCOMPARE(installer->kind, TreeNodeKind::Category);
    QCOMPARE(installer->children.size(), std::size_t{1});
    QCOMPARE(installer->children.front().kind, TreeNodeKind::Addon);
}

void FilesystemScannerTest::WhatTheImporterBroughtInIsAnAddonEvenWhenItHoldsOne()
{
    const Library library;
    library.AddManifest("Utils/flybywire-current-install-804/restore", R"({"title": "A380X"})");
    library.AddManifest("Utils/navigraph-nav-base", R"({"title": "Navdata"})");

    class TheImporterBrought final : public ImportedFolders
    {
    public:
        explicit TheImporterBrought(std::filesystem::path folder) : folder_(std::move(folder))
        {
        }

        [[nodiscard]] std::vector<std::filesystem::path> WhatTheImporterBrought() const override
        {
            return {folder_};
        }

    private:
        std::filesystem::path folder_;
    };

    const TheImporterBrought brought(library.Root() / "Utils" / "flybywire-current-install-804");
    const FilesystemScanner scanner(parser, probe, brought);
    const TreeNode root = scanner.Scan(library.Root());

    const TreeNode* utils = ChildNamed(root, "Utils");
    QVERIFY(utils != nullptr);
    QCOMPARE(utils->kind, TreeNodeKind::Category);

    const TreeNode* installer = ChildNamed(*utils, "flybywire-current-install-804");
    QVERIFY(installer != nullptr);
    QCOMPARE(installer->kind, TreeNodeKind::Addon);
    QVERIFY2(installer->children.empty(), "the walk kept exposing the package inside what the importer brought in");
}

void FilesystemScannerTest::AnEmptyFolderIsAnAddonUnlessTheMarkerDeclaresIt()
{
    const Library library;
    library.AddFolder("Liveries");
    library.Declare("Categoria Vazia");
    library.AddManifest("Sceneries/tlc-bgjn", R"({"title": "Ilulissat"})");

    const FilesystemScanner scanner(parser, probe, nothingWasImported);
    const TreeNode root = scanner.Scan(library.Root());

    QCOMPARE(root.children.size(), std::size_t{3});

    const TreeNode* liveries = ChildNamed(root, "Liveries");
    QVERIFY(liveries != nullptr);
    QCOMPARE(liveries->kind, TreeNodeKind::Addon);
    QVERIFY(liveries->addon.has_value());
    QCOMPARE(liveries->addon->manifest.title, std::string());

    const TreeNode* declared = ChildNamed(root, "Categoria Vazia");
    QVERIFY(declared != nullptr);
    QCOMPARE(declared->kind, TreeNodeKind::Category);
    QVERIFY(!declared->addon.has_value());
}

void FilesystemScannerTest::ACategoryTheAppDeclaredSurvivesLosingItsLastAddon()
{
    const Library library;
    library.Declare("Utils");
    library.AddFolder("Utils/navigraph-efb-chartsapp");

    const FilesystemScanner scanner(parser, probe, nothingWasImported);
    const TreeNode root = scanner.Scan(library.Root());

    const TreeNode* utils = ChildNamed(root, "Utils");
    QVERIFY(utils != nullptr);
    QCOMPARE(utils->kind, TreeNodeKind::Category);
    QVERIFY(utils->declaredAsCategory);

    const TreeNode* leftover = ChildNamed(*utils, "navigraph-efb-chartsapp");
    QVERIFY(leftover != nullptr);
    QCOMPARE(leftover->kind, TreeNodeKind::Addon);
    QVERIFY(!leftover->declaredAsCategory);
}

void FilesystemScannerTest::EveryCategoryTheScanReturnsHoldsAddonsOrWasDeclared()
{
    const Library library;
    library.AddManifest("Aircrafts/pmdg-aircraft-738", R"({"title": "PMDG 737"})");
    library.AddManifest("Sceneries/Brazil/tlc-sbgl", R"({"title": "Galeao"})");
    library.AddFolder("Liveries");
    library.AddFolder("Utils/ModelLib/CH47");
    library.Declare("Sounds");

    const FilesystemScanner scanner(parser, probe, nothingWasImported);
    const TreeNode root = scanner.Scan(library.Root());

    std::vector<const TreeNode*> pending{&root};
    std::size_t categories = 0;

    while (!pending.empty())
    {
        const TreeNode* node = pending.back();
        pending.pop_back();

        if (node->kind == TreeNodeKind::Category)
        {
            ++categories;
            QVERIFY2(HoldsAddonsOrWasDeclared(*node),
                     qPrintable(QStringLiteral("the scan returned a category that groups nothing and nobody "
                                               "declared: %1")
                                    .arg(QString::fromStdString(node->path.string()))));
        }

        for (const TreeNode& child : node->children)
        {
            pending.push_back(&child);
        }
    }

    QCOMPARE(categories, std::size_t{4});
}

void FilesystemScannerTest::AFolderWithAnUnreadableManifestIsStillAnAddon()
{
    const Library library;
    library.AddManifest("Sceneries/half-written-addon", "{\"title\":");

    const FilesystemScanner scanner(parser, probe, nothingWasImported);
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

    const FilesystemScanner scanner(parser, probe, nothingWasImported);
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
