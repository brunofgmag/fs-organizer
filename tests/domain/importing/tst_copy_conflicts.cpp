#include <QtTest/QtTest>

#include "domain/importing/CopyConflicts.h"
#include "tests/support/PathPrinting.h"

class CopyConflictsTest : public QObject
{
    Q_OBJECT

private slots:
    static void APhysicalFolderThatAlsoExistsInTheLibraryIsAConflict();
    static void AnEmptyFolderInTheLibraryIsNotAnAddonAndSoIsNotAConflict();
    static void AManagedLinkIsNotAPhysicalCopyAndSoIsNotAConflict();
    static void TheBaseNameIsComparedWithoutCaringAboutCase();
    static void TheSameNameInTwoDestinationsIsTwoConflicts();
};

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
        node.addon = Addon{path, Manifest{}};

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
        return DestinationEntry{path, {}, EntryClassification::Unmanaged};
    }
}

void CopyConflictsTest::APhysicalFolderThatAlsoExistsInTheLibraryIsAConflict()
{
    const std::vector<TreeNode> libraries{
        LibraryWith({Category(kLibrary / "Utils", {AddonNode(kLibrary / "Utils/flybywire-externaltools-simbridge")})})
    };

    const CopyConflicts conflicts = FindCopyConflicts(
        {PhysicalFolder(kCommunity / "flybywire-externaltools-simbridge")}, libraries);

    QCOMPARE(conflicts.Count(), std::size_t{1});

    const CopyConflict* found = conflicts.OverTheDestinationEntry(kCommunity / "flybywire-externaltools-simbridge");
    QVERIFY(found != nullptr);
    QCOMPARE(found->destinationPath, kCommunity / "flybywire-externaltools-simbridge");
    QCOMPARE(found->libraryPath, kLibrary / "Utils/flybywire-externaltools-simbridge");

    QCOMPARE(conflicts.OverTheLibraryAddon(kLibrary / "Utils/flybywire-externaltools-simbridge"), found);
}

void CopyConflictsTest::AnEmptyFolderInTheLibraryIsNotAnAddonAndSoIsNotAConflict()
{
    const std::vector<TreeNode> libraries{
        LibraryWith({Category(kLibrary / "Utils", {Category(kLibrary / "Utils/fs2crew-cmd-center", {})})})
    };

    const CopyConflicts conflicts =
        FindCopyConflicts({PhysicalFolder(kCommunity / "fs2crew-cmd-center")}, libraries);

    QCOMPARE(conflicts.Count(), std::size_t{0});
    QCOMPARE(conflicts.OverTheDestinationEntry(kCommunity / "fs2crew-cmd-center"), nullptr);
}

void CopyConflictsTest::AManagedLinkIsNotAPhysicalCopyAndSoIsNotAConflict()
{
    const std::vector<TreeNode> libraries{
        LibraryWith({Category(kLibrary / "Utils", {AddonNode(kLibrary / "Utils/simbridge")})})
    };

    const std::vector<DestinationEntry> entries{
        DestinationEntry{kCommunity / "simbridge", kLibrary / "Utils/simbridge", EntryClassification::Managed}
    };

    QCOMPARE(FindCopyConflicts(entries, libraries).Count(), std::size_t{0});
}

void CopyConflictsTest::TheBaseNameIsComparedWithoutCaringAboutCase()
{
    const std::vector<TreeNode> libraries{
        LibraryWith({Category(kLibrary / "Utils", {AddonNode(kLibrary / "Utils/SimBridge")})})
    };

    const CopyConflicts conflicts = FindCopyConflicts({PhysicalFolder(kCommunity / "simbridge")}, libraries);

    QCOMPARE(conflicts.Count(), std::size_t{1});
    QVERIFY(conflicts.OverTheLibraryAddon(kLibrary / "Utils/simbridge") != nullptr);
}

void CopyConflictsTest::TheSameNameInTwoDestinationsIsTwoConflicts()
{
    const std::vector<TreeNode> libraries{
        LibraryWith({Category(kLibrary / "Utils", {AddonNode(kLibrary / "Utils/simbridge")})})
    };

    const CopyConflicts conflicts = FindCopyConflicts(
        {PhysicalFolder(kCommunity / "simbridge"), PhysicalFolder(kCommunity2024 / "simbridge")}, libraries);

    QCOMPARE(conflicts.Count(), std::size_t{2});
    QVERIFY(conflicts.OverTheDestinationEntry(kCommunity / "simbridge") != nullptr);
    QVERIFY(conflicts.OverTheDestinationEntry(kCommunity2024 / "simbridge") != nullptr);
}

QTEST_APPLESS_MAIN(CopyConflictsTest)

#include "tst_copy_conflicts.moc"
