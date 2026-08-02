#include <QtTest/QtTest>

#include "domain/linking/EntryClassifier.h"
#include "domain/tree/AddonTree.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

class AddonTreeTest : public QObject
{
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
        static void EveryFolderThatCanHoldAnImportIsACategoryIncludingTheLibraryRoot();
        static void CountingCategoriesLeavesOutTheLibraryRootAndEveryAddon();
        static void AnEmptyFolderOnlyCountsAsACategoryOnceTheUserHasDeclaredIt();
        static void TheLibrariesAnswerWhichOfThemHoldsAnAddonOfAGivenName();
        static void AnIdentityNoAddonAnswersToIsFree();
        static void AnIdentityHeldInAnotherCategorySaysWhereTheOccupantIs();
        static void TheSameNameInAnotherLibraryLeavesTheIdentityFree();
        static void TheIdentityIsAskedWithoutDistinguishingCase();
        static void TheAddonBeingMovedDoesNotHoldTheIdentityAgainstItself();
        static void AFolderOutsideEveryLibraryHasNoIdentityToTake();
        static void TheLibrariesAnswerWhichTreeStandsAtAGivenRoot();
        static void ARootIsMatchedWithoutCaseOrSeparatorDifferences();
        static void ARootThatIsNoLibraryOfItsOwnHasNoTree();
    };
}

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
        return LibraryNode(
            "D:/MSFS 2024",
            {CategoryNode("D:/MSFS 2024/Aircrafts",
                          {AddonNode("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"),
                           CategoryNode("D:/MSFS 2024/Aircrafts/Fenix",
                                        {AddonNode("D:/MSFS 2024/Aircrafts/Fenix/fenix-a320")})}),
             CategoryNode("D:/MSFS 2024/Sceneries", {AddonNode("D:/MSFS 2024/Sceneries/ag-airport-bgqq-qaanaaq")})});
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
    const EnabledAddons enabled(
        {"D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w", "D:/MSFS 2024/Aircrafts/Fenix/fenix-a320"});

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
    const EnabledAddons everything({"D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w",
                                    "D:/MSFS 2024/Aircrafts/Fenix/fenix-a320",
                                    "D:/MSFS 2024/Sceneries/ag-airport-bgqq-qaanaaq"});
    const EnabledAddons oneCategory(
        {"D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w", "D:/MSFS 2024/Aircrafts/Fenix/fenix-a320"});

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
    const FakeFilesystemProbe filesystemProbe(fileSystem);
    const EntryClassifier classifier(linkService, filesystemProbe);

    const std::vector<DestinationEntry> entries = classifier.Resolve(
        {"E:/Flight Simulator 2024/Community", "E:/Flight Simulator 2024/Community2024"}, {"D:/MSFS 2024"});

    QCOMPARE(entries.front().classification, EntryClassification::Duplicated);
    QCOMPARE(DeriveCheckState(AddonNode(folder), EnabledAddons(EnabledAddonFolders(entries))), CheckState::Checked);
}

void AddonTreeTest::ATargetThatCameBackWithATrailingSeparatorStillMatchesItsAddon()
{
    const std::filesystem::path folder = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";

    QCOMPARE(DeriveCheckState(AddonNode(folder), EnabledAddons({"D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/"})),
             CheckState::Checked);
    QCOMPARE(DeriveCheckState(AddonNode(folder), EnabledAddons({R"(D:\MSFS 2024\Aircrafts\pmdg-aircraft-77w\)"})),
             CheckState::Checked);
}

void AddonTreeTest::AnEmptyFolderOnlyCountsAsACategoryOnceTheUserHasDeclaredIt()
{
    const TreeNode leftover = CategoryNode("D:/MSFS 2024/Utils/navigraph-efb-chartsapp", {});
    QVERIFY(!HoldsAddonsOrWasDeclared(leftover));

    TreeNode declared = leftover;
    declared.declaredAsCategory = true;
    QVERIFY(HoldsAddonsOrWasDeclared(declared));

    const TreeNode populated = CategoryNode("D:/MSFS 2024/Aircrafts", {AddonNode("D:/MSFS 2024/Aircrafts/fenix-a320")});
    QVERIFY(HoldsAddonsOrWasDeclared(populated));
}

void AddonTreeTest::EveryFolderThatCanHoldAnImportIsACategoryIncludingTheLibraryRoot()
{
    const TreeNode library = ReferenceLibrary();

    const std::vector<const TreeNode*> categories = CategoriesUnder(library);

    QCOMPARE(categories.size(), std::size_t{4});
    QCOMPARE(categories[0]->path, std::filesystem::path("D:/MSFS 2024"));
    QCOMPARE(categories[1]->path, std::filesystem::path("D:/MSFS 2024/Aircrafts"));
    QCOMPARE(categories[2]->path, std::filesystem::path("D:/MSFS 2024/Aircrafts/Fenix"));
    QCOMPARE(categories[3]->path, std::filesystem::path("D:/MSFS 2024/Sceneries"));
}

void AddonTreeTest::CountingCategoriesLeavesOutTheLibraryRootAndEveryAddon()
{
    const TreeNode library = ReferenceLibrary();

    QCOMPARE(CategoriesUnder(library).size(), std::size_t{4});
    QCOMPARE(CountCategoriesInside(library), std::size_t{3});

    TreeNode flat;
    flat.kind = TreeNodeKind::Library;
    flat.path = "D:/MSFS 2024/Aircrafts";
    flat.children = {AddonNode("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"),
                     AddonNode("D:/MSFS 2024/Aircrafts/fenix-a320")};

    QCOMPARE(CountAddons(flat), std::size_t{2});
    QCOMPARE(CountCategoriesInside(flat), std::size_t{0});
}

void AddonTreeTest::TheLibrariesAnswerWhichOfThemHoldsAnAddonOfAGivenName()
{
    const std::vector<TreeNode> libraries{ReferenceLibrary()};

    const TreeNode* found = AddonNamed(libraries, "PMDG-Aircraft-77W");
    QVERIFY(found != nullptr);
    QCOMPARE(found->path, std::filesystem::path("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"));

    QCOMPARE(AddonNamed(libraries, "Aircrafts"), nullptr);
    QCOMPARE(AddonNamed(libraries, "never-installed"), nullptr);
}

void AddonTreeTest::AnIdentityNoAddonAnswersToIsFree()
{
    const std::vector<TreeNode> libraries{ReferenceLibrary()};

    QCOMPARE(AddonHoldingTheIdentity(libraries, "D:/MSFS 2024/Sceneries/never-installed", {}), nullptr);
}

void AddonTreeTest::AnIdentityHeldInAnotherCategorySaysWhereTheOccupantIs()
{
    const std::vector<TreeNode> libraries{ReferenceLibrary()};

    const TreeNode* occupant = AddonHoldingTheIdentity(libraries, "D:/MSFS 2024/Sceneries/pmdg-aircraft-77w", {});

    QVERIFY(occupant != nullptr);
    QCOMPARE(occupant->path, std::filesystem::path("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"));
}

void AddonTreeTest::TheSameNameInAnotherLibraryLeavesTheIdentityFree()
{
    const std::vector<TreeNode> libraries{
        ReferenceLibrary(),
        LibraryNode("F:/Spare", {CategoryNode("F:/Spare/Aircrafts", {AddonNode("F:/Spare/Aircrafts/fenix-a320")})})};

    const TreeNode* occupant = AddonHoldingTheIdentity(libraries, "D:/MSFS 2024/Sceneries/fenix-a320", {});

    QVERIFY(occupant != nullptr);
    QCOMPARE(occupant->path, std::filesystem::path("D:/MSFS 2024/Aircrafts/Fenix/fenix-a320"));
    QCOMPARE(AddonHoldingTheIdentity(libraries, "F:/Spare/Sceneries/pmdg-aircraft-77w", {}), nullptr);
}

void AddonTreeTest::TheIdentityIsAskedWithoutDistinguishingCase()
{
    const std::vector<TreeNode> libraries{ReferenceLibrary()};

    const TreeNode* occupant = AddonHoldingTheIdentity(libraries, R"(d:\msfs 2024\Sceneries\PMDG-Aircraft-77W)", {});

    QVERIFY(occupant != nullptr);
    QCOMPARE(occupant->path, std::filesystem::path("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"));
}

void AddonTreeTest::TheAddonBeingMovedDoesNotHoldTheIdentityAgainstItself()
{
    const std::vector<TreeNode> libraries{ReferenceLibrary()};

    QCOMPARE(AddonHoldingTheIdentity(libraries, "D:/MSFS 2024/Sceneries/pmdg-aircraft-77w",
                                     "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"),
             nullptr);
}

void AddonTreeTest::AFolderOutsideEveryLibraryHasNoIdentityToTake()
{
    const std::vector<TreeNode> libraries{ReferenceLibrary()};

    QCOMPARE(AddonHoldingTheIdentity(libraries, "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", {}), nullptr);
}

void AddonTreeTest::TheLibrariesAnswerWhichTreeStandsAtAGivenRoot()
{
    const std::vector<TreeNode> libraries{
        ReferenceLibrary(),
        LibraryNode("F:/Spare", {CategoryNode("F:/Spare/Aircrafts", {AddonNode("F:/Spare/Aircrafts/fenix-a320")})})};

    const TreeNode* spare = LibraryTreeAt(libraries, "F:/Spare");
    const TreeNode* reference = LibraryTreeAt(libraries, "D:/MSFS 2024");

    QVERIFY(spare != nullptr);
    QVERIFY(reference != nullptr);
    QCOMPARE(spare->path, std::filesystem::path("F:/Spare"));
    QCOMPARE(reference->path, std::filesystem::path("D:/MSFS 2024"));
}

void AddonTreeTest::ARootIsMatchedWithoutCaseOrSeparatorDifferences()
{
    const std::vector<TreeNode> libraries{ReferenceLibrary()};

    const TreeNode* byBackslash = LibraryTreeAt(libraries, R"(d:\msfs 2024)");
    const TreeNode* byTrailingSeparator = LibraryTreeAt(libraries, "D:/MSFS 2024/");

    QVERIFY(byBackslash != nullptr);
    QVERIFY(byTrailingSeparator != nullptr);
    QCOMPARE(byBackslash->path, std::filesystem::path("D:/MSFS 2024"));
    QCOMPARE(byTrailingSeparator->path, std::filesystem::path("D:/MSFS 2024"));
}

void AddonTreeTest::ARootThatIsNoLibraryOfItsOwnHasNoTree()
{
    const std::vector<TreeNode> libraries{ReferenceLibrary()};

    QCOMPARE(LibraryTreeAt(libraries, "D:/MSFS 2024/Aircrafts"), nullptr);
    QCOMPARE(LibraryTreeAt(libraries, "F:/Spare"), nullptr);
}

QTEST_APPLESS_MAIN(AddonTreeTest)

#include "tst_addon_tree.moc"
