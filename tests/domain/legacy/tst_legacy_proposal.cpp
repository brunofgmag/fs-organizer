#include <QtTest/QtTest>

#include "domain/legacy/LegacyProposal.h"
#include "domain/support/PathUtils.h"
#include "tests/support/PathPrinting.h"

class LegacyProposalTest : public QObject
{
    class LegacyProposalTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void SiblingEntriesBecomeOneLibraryWithACategoryEach();
        static void TheCategoryIsTheEntryRelativeToTheRoot();
        static void EntriesAtDifferentDepthsShareTheirCommonAncestor();
        static void EntriesOnDifferentVolumesEachBecomeTheirOwnLibrary();
        static void ACommonAncestorThatIsOnlyTheDriveRootIsRefused();
        static void AnEntryThatIsItselfTheAncestorBecomesTheRootAndNotACategory();
        static void RepeatedEntriesProposeTheCategoryOnce();
        static void CaseAndSeparatorDifferencesDoNotSplitTheAncestor();
        static void TheCategoryKeepsTheCaseTheEntryHad();
        static void ALibraryAlreadyRegisteredIsMarkedAsSuch();
        static void ACategoryAlreadyOnDiskIsMarkedAsSuch();
        static void AnEmptyFolderTheIniNamesIsOfferedInsteadOfTakenAsPresent();
        static void EveryCategoryOfANewLibraryIsNew();
        static void AnEntryWithAnInvalidSegmentIsRefusedInsteadOfProposed();
        static void NoEntriesProposeNoLibrary();
    };
}

namespace
{
    LegacyInstallation Naming(const std::vector<std::filesystem::path>& addonPaths)
    {
        LegacyInstallation installation;
        installation.addonPaths = addonPaths;

        return installation;
    }

    std::vector<std::filesystem::path> ReferenceEntriesOf2024()
    {
        return {"D:/MSFS 2024/Aircraft Mods", "D:/MSFS 2024/Aircrafts", "D:/MSFS 2024/Liveries",
                "D:/MSFS 2024/Sceneries",     "D:/MSFS 2024/Sounds",    "D:/MSFS 2024/Traffic",
                "D:/MSFS 2024/Utils"};
    }

    SimulatorProfile ProfileHolding(const std::vector<std::filesystem::path>& libraryPaths)
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";

        for (const std::filesystem::path& path : libraryPaths)
        {
            profile.libraries.push_back(Library{"library-1", path, "MSFS 2024"});
        }

        return profile;
    }

    TreeNode CategoryAt(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Category;
        node.path = path;

        return node;
    }

    TreeNode CategoryHolding(const std::filesystem::path& path)
    {
        TreeNode addon;
        addon.kind = TreeNodeKind::Addon;
        addon.path = path / "an-addon";

        TreeNode category = CategoryAt(path);
        category.children.push_back(std::move(addon));

        return category;
    }

    std::vector<TreeNode> LibraryScannedAt(const std::filesystem::path& root,
                                           const std::vector<std::filesystem::path>& categories)
    {
        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = root;

        for (const std::filesystem::path& category : categories)
        {
            library.children.push_back(CategoryHolding(category));
        }

        return {library};
    }

    std::vector<TreeNode> AlsoHoldingTheEmptyFolder(std::vector<TreeNode> scanned, const std::filesystem::path& folder)
    {
        scanned.front().children.push_back(CategoryAt(folder));

        return scanned;
    }
}

void LegacyProposalTest::SiblingEntriesBecomeOneLibraryWithACategoryEach()
{
    const std::vector<ProposedLibrary> proposed = ProposeLibraries(Naming(ReferenceEntriesOf2024()), {}, {});

    QCOMPARE(proposed.size(), std::size_t{1});
    QCOMPARE(ComparablePath(proposed.front().root), ComparablePath("D:/MSFS 2024"));
    QCOMPARE(proposed.front().categories.size(), std::size_t{7});
    QVERIFY(proposed.front().refused.empty());
}

void LegacyProposalTest::TheCategoryIsTheEntryRelativeToTheRoot()
{
    const std::vector<ProposedLibrary> proposed = ProposeLibraries(Naming(ReferenceEntriesOf2024()), {}, {});

    QCOMPARE(ComparablePath(proposed.front().categories.front().relativePath), std::string{"aircraft mods"});
    QCOMPARE(ComparablePath(proposed.front().categories.back().relativePath), std::string{"utils"});
}

void LegacyProposalTest::EntriesAtDifferentDepthsShareTheirCommonAncestor()
{
    const std::vector<ProposedLibrary> proposed =
        ProposeLibraries(Naming({"D:/MSFS 2024/Aircrafts", "D:/MSFS 2024/Extra/Liveries"}), {}, {});

    QCOMPARE(proposed.size(), std::size_t{1});
    QCOMPARE(ComparablePath(proposed.front().root), ComparablePath("D:/MSFS 2024"));
    QCOMPARE(ComparablePath(proposed.front().categories[1].relativePath), std::string{"extra/liveries"});
}

void LegacyProposalTest::EntriesOnDifferentVolumesEachBecomeTheirOwnLibrary()
{
    const std::vector<ProposedLibrary> proposed =
        ProposeLibraries(Naming({"D:/Addons/Aircrafts", "E:/Somewhere Else/Sceneries"}), {}, {});

    QCOMPARE(proposed.size(), std::size_t{2});
    QCOMPARE(ComparablePath(proposed[0].root), ComparablePath("D:/Addons/Aircrafts"));
    QVERIFY(proposed[0].categories.empty());
    QCOMPARE(ComparablePath(proposed[1].root), ComparablePath("E:/Somewhere Else/Sceneries"));
}

void LegacyProposalTest::ACommonAncestorThatIsOnlyTheDriveRootIsRefused()
{
    const std::vector<ProposedLibrary> proposed = ProposeLibraries(Naming({"D:/Addons", "D:/Mods"}), {}, {});

    QCOMPARE(proposed.size(), std::size_t{2});
    QCOMPARE(ComparablePath(proposed[0].root), ComparablePath("D:/Addons"));
    QCOMPARE(ComparablePath(proposed[1].root), ComparablePath("D:/Mods"));
}

void LegacyProposalTest::AnEntryThatIsItselfTheAncestorBecomesTheRootAndNotACategory()
{
    const std::vector<ProposedLibrary> proposed =
        ProposeLibraries(Naming({"D:/MSFS 2024", "D:/MSFS 2024/Aircrafts"}), {}, {});

    QCOMPARE(proposed.size(), std::size_t{1});
    QCOMPARE(ComparablePath(proposed.front().root), ComparablePath("D:/MSFS 2024"));
    QCOMPARE(proposed.front().categories.size(), std::size_t{1});
    QCOMPARE(ComparablePath(proposed.front().categories.front().relativePath), std::string{"aircrafts"});
}

void LegacyProposalTest::RepeatedEntriesProposeTheCategoryOnce()
{
    const std::vector<ProposedLibrary> proposed = ProposeLibraries(
        Naming({"D:/MSFS 2024/Aircrafts", "D:/MSFS 2024/Aircrafts", "D:/MSFS 2024/Sceneries"}), {}, {});

    QCOMPARE(proposed.size(), std::size_t{1});
    QCOMPARE(proposed.front().categories.size(), std::size_t{2});
}

void LegacyProposalTest::CaseAndSeparatorDifferencesDoNotSplitTheAncestor()
{
    const std::vector<ProposedLibrary> proposed =
        ProposeLibraries(Naming({R"(D:\MSFS 2024\Aircrafts)", "d:/msfs 2024/Sceneries"}), {}, {});

    QCOMPARE(proposed.size(), std::size_t{1});
    QCOMPARE(proposed.front().categories.size(), std::size_t{2});
    QCOMPARE(ComparablePath(proposed.front().root), ComparablePath("D:/MSFS 2024"));
    QVERIFY(proposed.front().refused.empty());
}

void LegacyProposalTest::TheCategoryKeepsTheCaseTheEntryHad()
{
    const std::vector<ProposedLibrary> proposed =
        ProposeLibraries(Naming({"D:/MSFS 2024/Aircrafts", "D:/MSFS 2024/Aircrafts (2024)"}), {}, {});

    QCOMPARE(proposed.front().categories[1].relativePath.string(), std::string{"Aircrafts (2024)"});
}

void LegacyProposalTest::ALibraryAlreadyRegisteredIsMarkedAsSuch()
{
    const std::vector<ProposedLibrary> proposed =
        ProposeLibraries(Naming(ReferenceEntriesOf2024()), ProfileHolding({"D:/MSFS 2024"}), {});

    QCOMPARE(proposed.front().state, ProposedState::AlreadyPresent);
}

void LegacyProposalTest::ACategoryAlreadyOnDiskIsMarkedAsSuch()
{
    const std::vector<ProposedLibrary> proposed =
        ProposeLibraries(Naming({"D:/MSFS 2024/Aircrafts", "D:/MSFS 2024/Sceneries"}), ProfileHolding({"D:/MSFS 2024"}),
                         LibraryScannedAt("D:/MSFS 2024", {"D:/MSFS 2024/Aircrafts"}));

    QCOMPARE(proposed.front().categories[0].state, ProposedState::AlreadyPresent);
    QCOMPARE(proposed.front().categories[1].state, ProposedState::New);
}

void LegacyProposalTest::AnEmptyFolderTheIniNamesIsOfferedInsteadOfTakenAsPresent()
{
    const std::vector<ProposedLibrary> proposed = ProposeLibraries(
        Naming({"D:/MSFS 2024/Aircrafts", "D:/MSFS 2024/Sounds"}), ProfileHolding({"D:/MSFS 2024"}),
        AlsoHoldingTheEmptyFolder(LibraryScannedAt("D:/MSFS 2024", {"D:/MSFS 2024/Aircrafts"}), "D:/MSFS 2024/Sounds"));

    QCOMPARE(proposed.front().categories[0].state, ProposedState::AlreadyPresent);
    QCOMPARE(proposed.front().categories[1].state, ProposedState::New);
}

void LegacyProposalTest::EveryCategoryOfANewLibraryIsNew()
{
    const std::vector<ProposedLibrary> proposed =
        ProposeLibraries(Naming({"D:/MSFS 2024/Aircrafts", "D:/MSFS 2024/Sceneries"}), {},
                         LibraryScannedAt("D:/MSFS 2024", {"D:/MSFS 2024/Aircrafts"}));

    QCOMPARE(proposed.front().state, ProposedState::New);
    QCOMPARE(proposed.front().categories[0].state, ProposedState::New);
}

void LegacyProposalTest::AnEntryWithAnInvalidSegmentIsRefusedInsteadOfProposed()
{
    const std::vector<ProposedLibrary> proposed =
        ProposeLibraries(Naming({"D:/MSFS 2024/Aircrafts", "D:/MSFS 2024/Sceneries."}), {}, {});

    QCOMPARE(proposed.front().categories.size(), std::size_t{1});
    QCOMPARE(proposed.front().refused.size(), std::size_t{1});
    QCOMPARE(ComparablePath(proposed.front().refused.front()), ComparablePath("D:/MSFS 2024/Sceneries."));
}

void LegacyProposalTest::NoEntriesProposeNoLibrary()
{
    QVERIFY(ProposeLibraries(Naming({}), {}, {}).empty());
}

QTEST_APPLESS_MAIN(LegacyProposalTest)

#include "tst_legacy_proposal.moc"
