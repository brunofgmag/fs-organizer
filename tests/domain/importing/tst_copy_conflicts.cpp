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
                                            .theOtherProgramTookItsFolderBack = true}},
                          libraries);

    QCOMPARE(conflicts.Count(), std::size_t{1});
    QVERIFY(conflicts.OverTheLibraryAddon(libraryCopy) != nullptr);
    QVERIFY(conflicts.OverTheLibraryAddon(libraryCopy)->theProvenanceIsAnotherProgram);
}

QTEST_APPLESS_MAIN(CopyConflictsTest)

#include "tst_copy_conflicts.moc"
