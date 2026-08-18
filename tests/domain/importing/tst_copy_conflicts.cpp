#include <QtTest/QtTest>

#include "domain/importing/CopyConflicts.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class CopyConflictsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void APhysicalFolderThatAlsoExistsInTheLibraryIsAConflict();
        static void AnEmptyFolderInTheLibraryIsNotAnAddonAndSoIsNotAConflict();
        static void AManagedLinkIsNotAPhysicalCopyAndSoIsNotAConflict();
        static void TheBaseNameIsComparedWithoutCaringAboutCase();
        static void TheSameNameInTwoDestinationsIsTwoConflicts();
        static void ADivergentEntryIsAConflictBetweenTheLibraryAndTheOtherProgramsFolder();
        static void AVanishedEntryIsNotAConflictBecauseOnlyOneCopyIsLeft();
        static void ADuplicatedEntryStillCarriesItsDivergenceIntoTheConflict();
        static void AnEntryPointedBackAtItsVendorStillPutsTheLibraryCopyOnTheOtherSide();
        static void ASubstitutedEntryTakesTheLibraryCopyTheJournalNamesAndNotOneGuessedByName();
        static void ASubstitutedConflictSaysTheLinkWasReplacedSoTheScreenCanOfferTheTakeBack();
    };
}

namespace
{
    const std::filesystem::path kCommunity = "E:/Sim/Community";
    const std::filesystem::path kCommunity2024 = "E:/Sim/Community2024";
    const std::filesystem::path kLibrary = "D:/Library";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    TreeNode Category(const std::filesystem::path& path, std::vector<TreeNode> children)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Category;
        node.path = path;
        node.children = std::move(children);

        return node;
    }

    TreeNode LibraryWith(std::vector<TreeNode> children)
    {
        TreeNode node = Category(kLibrary, std::move(children));
        node.kind = TreeNodeKind::Library;

        return node;
    }

    DestinationEntry PhysicalFolder(const std::filesystem::path& path)
    {
        return DestinationEntry{.path = path, .target = {}, .classification = EntryClassification::Unmanaged};
    }
}

void CopyConflictsTest::APhysicalFolderThatAlsoExistsInTheLibraryIsAConflict()
{
    const std::vector<TreeNode> libraries{
        LibraryWith({Category(kLibrary / "Utils", {AddonNode(kLibrary / "Utils/flybywire-externaltools-simbridge")})})};

    const CopyConflicts conflicts =
        FindCopyConflicts({PhysicalFolder(kCommunity / "flybywire-externaltools-simbridge")}, libraries);

    QCOMPARE(conflicts.Count(), std::size_t{1});

    const CopyConflict* found = conflicts.OverTheProvenance(kCommunity / "flybywire-externaltools-simbridge");
    QVERIFY(found != nullptr);
    QCOMPARE(found->provenancePath, kCommunity / "flybywire-externaltools-simbridge");
    QCOMPARE(found->libraryPath, kLibrary / "Utils/flybywire-externaltools-simbridge");

    QCOMPARE(conflicts.OverTheLibraryAddon(kLibrary / "Utils/flybywire-externaltools-simbridge"), found);
}

void CopyConflictsTest::AnEmptyFolderInTheLibraryIsNotAnAddonAndSoIsNotAConflict()
{
    const std::vector<TreeNode> libraries{
        LibraryWith({Category(kLibrary / "Utils", {Category(kLibrary / "Utils/fs2crew-cmd-center", {})})})};

    const CopyConflicts conflicts = FindCopyConflicts({PhysicalFolder(kCommunity / "fs2crew-cmd-center")}, libraries);

    QCOMPARE(conflicts.Count(), std::size_t{0});
    QCOMPARE(conflicts.OverTheProvenance(kCommunity / "fs2crew-cmd-center"), nullptr);
}

void CopyConflictsTest::AManagedLinkIsNotAPhysicalCopyAndSoIsNotAConflict()
{
    const std::vector<TreeNode> libraries{
        LibraryWith({Category(kLibrary / "Utils", {AddonNode(kLibrary / "Utils/simbridge")})})};

    const std::vector<DestinationEntry> entries{DestinationEntry{.path = kCommunity / "simbridge",
                                                                 .target = kLibrary / "Utils/simbridge",
                                                                 .classification = EntryClassification::Managed}};

    QCOMPARE(FindCopyConflicts(entries, libraries).Count(), std::size_t{0});
}

void CopyConflictsTest::TheBaseNameIsComparedWithoutCaringAboutCase()
{
    const std::vector<TreeNode> libraries{
        LibraryWith({Category(kLibrary / "Utils", {AddonNode(kLibrary / "Utils/SimBridge")})})};

    const CopyConflicts conflicts = FindCopyConflicts({PhysicalFolder(kCommunity / "simbridge")}, libraries);

    QCOMPARE(conflicts.Count(), std::size_t{1});
    QVERIFY(conflicts.OverTheLibraryAddon(kLibrary / "Utils/simbridge") != nullptr);
}

void CopyConflictsTest::TheSameNameInTwoDestinationsIsTwoConflicts()
{
    const std::vector<TreeNode> libraries{
        LibraryWith({Category(kLibrary / "Utils", {AddonNode(kLibrary / "Utils/simbridge")})})};

    const CopyConflicts conflicts = FindCopyConflicts(
        {PhysicalFolder(kCommunity / "simbridge"), PhysicalFolder(kCommunity2024 / "simbridge")}, libraries);

    QCOMPARE(conflicts.Count(), std::size_t{2});
    QVERIFY(conflicts.OverTheProvenance(kCommunity / "simbridge") != nullptr);
    QVERIFY(conflicts.OverTheProvenance(kCommunity2024 / "simbridge") != nullptr);
}

void CopyConflictsTest::ADivergentEntryIsAConflictBetweenTheLibraryAndTheOtherProgramsFolder()
{
    const std::filesystem::path vendorFolder = "C:/Program Files (x86)/Addon Manager/MSFS/gsx-pro";
    const std::filesystem::path libraryCopy = kLibrary / "Utils/gsx-pro";

    const std::vector<TreeNode> libraries{LibraryWith({Category(kLibrary / "Utils", {AddonNode(libraryCopy)})})};

    const CopyConflicts conflicts =
        FindCopyConflicts({DestinationEntry{.path = kCommunity / "gsx-pro",
                                            .target = libraryCopy,
                                            .classification = EntryClassification::Divergent,
                                            .externalOrigin = vendorFolder,
                                            .libraryCopy = libraryCopy,
                                            .theOtherProgramTookItsFolderBack = true}},
                          libraries);

    QCOMPARE(conflicts.Count(), std::size_t{1});

    const CopyConflict* found = conflicts.OverTheProvenance(vendorFolder);
    QVERIFY(found != nullptr);
    QCOMPARE(found->provenancePath, vendorFolder);
    QCOMPARE(found->libraryPath, libraryCopy);

    QCOMPARE(conflicts.OverTheLibraryAddon(libraryCopy), found);
}

void CopyConflictsTest::AVanishedEntryIsNotAConflictBecauseOnlyOneCopyIsLeft()
{
    const std::vector<TreeNode> libraries{
        LibraryWith({Category(kLibrary / "Utils", {AddonNode(kLibrary / "Utils/gsx-pro")})})};

    const CopyConflicts conflicts =
        FindCopyConflicts({DestinationEntry{.path = kCommunity / "gsx-pro",
                                            .target = kLibrary / "Utils/gsx-pro",
                                            .classification = EntryClassification::Vanished,
                                            .externalOrigin = "C:/Program Files (x86)/Addon Manager/MSFS/gsx-pro"}},
                          libraries);

    QCOMPARE(conflicts.Count(), std::size_t{0});
}

void CopyConflictsTest::ADuplicatedEntryStillCarriesItsDivergenceIntoTheConflict()
{
    const std::filesystem::path vendorFolder = "C:/Program Files (x86)/Addon Manager/MSFS/gsx-pro";
    const std::filesystem::path libraryCopy = kLibrary / "Utils/gsx-pro";

    const std::vector<TreeNode> libraries{LibraryWith({Category(kLibrary / "Utils", {AddonNode(libraryCopy)})})};

    const CopyConflicts conflicts =
        FindCopyConflicts({DestinationEntry{.path = kCommunity / "gsx-pro",
                                            .target = libraryCopy,
                                            .classification = EntryClassification::Duplicated,
                                            .externalOrigin = vendorFolder,
                                            .libraryCopy = libraryCopy,
                                            .theOtherProgramTookItsFolderBack = true}},
                          libraries);

    QCOMPARE(conflicts.Count(), std::size_t{1});
    QVERIFY(conflicts.OverTheLibraryAddon(libraryCopy) != nullptr);
    QVERIFY(conflicts.OverTheLibraryAddon(libraryCopy)->theProvenanceIsAnotherProgram);
}

void CopyConflictsTest::AnEntryPointedBackAtItsVendorStillPutsTheLibraryCopyOnTheOtherSide()
{
    const std::filesystem::path vendorFolder = "C:/Program Files (x86)/Addon Manager/MSFS/gsx-pro";
    const std::filesystem::path libraryCopy = kLibrary / "Utils/gsx-pro";

    const std::vector<TreeNode> libraries{LibraryWith({Category(kLibrary / "Utils", {AddonNode(libraryCopy)})})};

    const CopyConflicts conflicts =
        FindCopyConflicts({DestinationEntry{.path = kCommunity / "gsx-pro",
                                            .target = vendorFolder,
                                            .classification = EntryClassification::Divergent,
                                            .externalOrigin = vendorFolder,
                                            .libraryCopy = libraryCopy,
                                            .theOtherProgramTookItsFolderBack = true}},
                          libraries);

    const CopyConflict* found = conflicts.OverTheProvenance(vendorFolder);
    QVERIFY(found != nullptr);
    QCOMPARE(found->libraryPath, libraryCopy);
    QVERIFY2(conflicts.OverTheLibraryAddon(libraryCopy) == found,
             "the library copy is still the addon the tree has to mark, even when no entry points at it");
}

void CopyConflictsTest::ASubstitutedEntryTakesTheLibraryCopyTheJournalNamesAndNotOneGuessedByName()
{
    const std::filesystem::path adrift = kLibrary / "Navdata/airac-base";

    const std::vector<TreeNode> libraries{LibraryWith({Category(kLibrary / "Navdata", {AddonNode(adrift)})})};

    const DestinationEntry substituted{.path = kCommunity / "navigraph-nav-base",
                                       .target = {},
                                       .classification = EntryClassification::Substituted,
                                       .externalOrigin = {},
                                       .libraryCopy = adrift};

    const CopyConflicts conflicts = FindCopyConflicts({substituted}, libraries);

    QCOMPARE(conflicts.Count(), std::size_t{1});

    const CopyConflict* found = conflicts.OverTheProvenance(kCommunity / "navigraph-nav-base");

    QVERIFY(found != nullptr);
    QVERIFY2(found->libraryPath == adrift,
             "the journal knows which folder the link pointed at, and a name that no longer matches does not");
}

void CopyConflictsTest::ASubstitutedConflictSaysTheLinkWasReplacedSoTheScreenCanOfferTheTakeBack()
{
    const std::filesystem::path adrift = kLibrary / "Utils/gsx-pro";

    const std::vector<TreeNode> libraries{LibraryWith({Category(kLibrary / "Utils", {AddonNode(adrift)})})};

    const DestinationEntry substituted{.path = kCommunity / "gsx-pro",
                                       .target = {},
                                       .classification = EntryClassification::Substituted,
                                       .externalOrigin = {},
                                       .libraryCopy = adrift};

    const CopyConflicts conflicts = FindCopyConflicts({substituted}, libraries);
    const CopyConflict* found = conflicts.OverTheProvenance(kCommunity / "gsx-pro");

    QVERIFY(found != nullptr);
    QVERIFY(found->ourLinkWasReplaced);
    QVERIFY2(!found->theProvenanceIsAnotherProgram,
             "nobody handed this folder over: it was ours until something wrote over it");
}

QTEST_APPLESS_MAIN(CopyConflictsTest)

#include "tst_copy_conflicts.moc"
