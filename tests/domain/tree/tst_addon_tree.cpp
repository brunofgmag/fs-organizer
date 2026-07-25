#include <QtTest/QtTest>

#include "domain/linking/EntryClassifier.h"
#include "domain/tree/AddonTree.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

class AddonTreeTest : public QObject
{
    Q_OBJECT

private slots:
    static void CountingAddonsSeesEveryDepthAndIgnoresCategories();
    static void MarkingACategoryReachesEveryAddonUnderIt();
    static void ACategoryWhoseAddonsAreAllEnabledIsChecked();
    static void ACategoryWithOneAddonDisabledIsPartial();
    static void ACategoryWithNoEnabledAddonIsUnchecked();
    static void ACategoryWithoutAddonsIsUnchecked();
    static void TheLibraryRootRollsUpFromEveryCategory();
    static void AnEnabledAddonIsCheckedAndTheLookupIgnoresCase();
    static void AnAddonLinkedInTwoDestinationsIsCheckedRatherThanUnchecked();
    static void ATargetThatCameBackWithATrailingSeparatorStillMatchesItsAddon();
};

namespace
{
    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{path, Manifest{}};

        return node;
    }

    TreeNode CategoryNode(const std::filesystem::path& path, std::vector<TreeNode> children)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Category;
        node.path = path;
        node.children = std::move(children);

        return node;
    }

    TreeNode LibraryNode(const std::filesystem::path& path, std::vector<TreeNode> children)
    {
        TreeNode node = CategoryNode(path, std::move(children));
        node.kind = TreeNodeKind::Library;

        return node;
    }

    TreeNode ReferenceLibrary()
    {
        return LibraryNode("D:/MSFS 2024", {
                               CategoryNode("D:/MSFS 2024/Aircrafts", {
                                                AddonNode("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"),
                                                CategoryNode("D:/MSFS 2024/Aircrafts/Fenix", {
                                                                 AddonNode("D:/MSFS 2024/Aircrafts/Fenix/fenix-a320")
                                                             })
                                            }),
                               CategoryNode("D:/MSFS 2024/Sceneries", {
                                                AddonNode("D:/MSFS 2024/Sceneries/ag-airport-bgqq-qaanaaq")
                                            })
                           });
    }
}

void AddonTreeTest::CountingAddonsSeesEveryDepthAndIgnoresCategories()
{
    QCOMPARE(CountAddons(ReferenceLibrary()), std::size_t{3});
    QCOMPARE(CountAddons(CategoryNode("D:/MSFS 2024/Empty", {})), std::size_t{0});
}

void AddonTreeTest::MarkingACategoryReachesEveryAddonUnderIt()
{
    const TreeNode library = ReferenceLibrary();

    const std::vector<const TreeNode*> addons = AddonsUnder(library.children.front());

    QCOMPARE(addons.size(), std::size_t{2});
    QCOMPARE(addons.front()->path, std::filesystem::path("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"));
    QCOMPARE(addons.back()->path, std::filesystem::path("D:/MSFS 2024/Aircrafts/Fenix/fenix-a320"));
}

void AddonTreeTest::ACategoryWhoseAddonsAreAllEnabledIsChecked()
{
    const TreeNode library = ReferenceLibrary();
    const EnabledAddons enabled({
        "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w",
        "D:/MSFS 2024/Aircrafts/Fenix/fenix-a320"
    });

    QCOMPARE(DeriveCheckState(library.children.front(), enabled), CheckState::Checked);
}

void AddonTreeTest::ACategoryWithOneAddonDisabledIsPartial()
{
    const TreeNode library = ReferenceLibrary();
    const EnabledAddons enabled({"D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"});

    QCOMPARE(DeriveCheckState(library.children.front(), enabled), CheckState::Partial);
}

void AddonTreeTest::ACategoryWithNoEnabledAddonIsUnchecked()
{
    const TreeNode library = ReferenceLibrary();
    const EnabledAddons enabled({"D:/MSFS 2024/Sceneries/ag-airport-bgqq-qaanaaq"});

    QCOMPARE(DeriveCheckState(library.children.front(), enabled), CheckState::Unchecked);
}

void AddonTreeTest::ACategoryWithoutAddonsIsUnchecked()
{
    const EnabledAddons enabled({"D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"});

    QCOMPARE(DeriveCheckState(CategoryNode("D:/MSFS 2024/Empty", {}), enabled), CheckState::Unchecked);
}

void AddonTreeTest::TheLibraryRootRollsUpFromEveryCategory()
{
    const TreeNode library = ReferenceLibrary();
    const EnabledAddons everything({
        "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w",
        "D:/MSFS 2024/Aircrafts/Fenix/fenix-a320",
        "D:/MSFS 2024/Sceneries/ag-airport-bgqq-qaanaaq"
    });
    const EnabledAddons oneCategory({
        "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w",
        "D:/MSFS 2024/Aircrafts/Fenix/fenix-a320"
    });

    QCOMPARE(DeriveCheckState(library, everything), CheckState::Checked);
    QCOMPARE(DeriveCheckState(library, oneCategory), CheckState::Partial);
    QCOMPARE(DeriveCheckState(library, EnabledAddons()), CheckState::Unchecked);
}

void AddonTreeTest::AnEnabledAddonIsCheckedAndTheLookupIgnoresCase()
{
    const std::filesystem::path folder = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";

    QCOMPARE(DeriveCheckState(AddonNode(folder), EnabledAddons({folder})), CheckState::Checked);
    QCOMPARE(DeriveCheckState(AddonNode(folder), EnabledAddons()), CheckState::Unchecked);
    QCOMPARE(DeriveCheckState(AddonNode(folder), EnabledAddons({"d:/msfs 2024/aircrafts/PMDG-Aircraft-77W"})),
             CheckState::Checked);
}

void AddonTreeTest::AnAddonLinkedInTwoDestinationsIsCheckedRatherThanUnchecked()
{
    const std::filesystem::path folder = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";

    InMemoryFileSystem fileSystem;
    fileSystem.AddDirectory(folder);
    fileSystem.AddDirectory("E:/Flight Simulator 2024/Community");
    fileSystem.AddDirectory("E:/Flight Simulator 2024/Community2024");
    fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", folder);
    fileSystem.AddLink("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w", folder);

    const FakeLinkService linkService(fileSystem);
    const FakeFileOperations fileOperations(fileSystem);
    const EntryClassifier classifier(linkService, fileOperations);

    const std::vector<DestinationEntry> entries = classifier.Resolve(
        {"E:/Flight Simulator 2024/Community", "E:/Flight Simulator 2024/Community2024"},
        {"D:/MSFS 2024"});

    QCOMPARE(entries.front().classification, EntryClassification::Duplicated);
    QCOMPARE(DeriveCheckState(AddonNode(folder), EnabledAddons(EnabledAddonFolders(entries))),
             CheckState::Checked);
}

void AddonTreeTest::ATargetThatCameBackWithATrailingSeparatorStillMatchesItsAddon()
{
    const std::filesystem::path folder = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";

    QCOMPARE(DeriveCheckState(AddonNode(folder),
                              EnabledAddons({"D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/"})),
             CheckState::Checked);
    QCOMPARE(DeriveCheckState(AddonNode(folder),
                              EnabledAddons({R"(D:\MSFS 2024\Aircrafts\pmdg-aircraft-77w\)"})),
             CheckState::Checked);
}

QTEST_APPLESS_MAIN(AddonTreeTest)

#include "tst_addon_tree.moc"
