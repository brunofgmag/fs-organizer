#include <QtTest/QtTest>

#include <algorithm>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/DestinationDivergence.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/AddonTreeViewModel.h"

class AddonTreeViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    static void ABlankCategoryNameIsRefusedBeforeItReachesTheJournal();
    static void ACategoryAskedForOnAnAddonLandsBesideItInsteadOfInsideIt();
    static void RenamingACategoryToTheNameItAlreadyHasIsNotAFailure();
    static void ARefusedRenameIsExplainedInsteadOfPassingSilently();
    static void MovingASelectionThatHoldsACategoryMovesOnlyTheAddonsInIt();
    static void MovingASelectionWithNoAddonInItIsRefusedBeforeItReachesTheJournal();
    static void TheCategoryAnAddonAlreadySitsInIsNotOfferedAsAMoveTarget();
    static void TheLibraryRootIsNotOfferedSoAMoveNeverLandsAnAddonLoose();
    static void AnEmptyFolderNobodyDeclaredIsLeftOutOfTheMoveTargets();
    static void ANestedCategoryIsOfferedByItsPathSoTwoOfTheSameNameStayApart();
    static void AnAddonInALibraryWithNoCategoryHasNowhereToBeMovedTo();
    static void AdoptingWritesTheOverrideOnTheCategoryWhenEveryEnabledAddonAgrees();
    static void AdoptingIsRefusedWhenTheEnabledAddonsPointAtDifferentDestinations();
    static void AdoptingIsRefusedWhenNoAddonInTheCategoryIsEnabled();
    static void OnlyACategoryHoldingAStrayAddonIsWorthOfferingTheAdoption();
    static void TurningAStrayAddonOffAndOnAgainLandsItInTheDestinationTheProfileMandates();
    static void RelinkingTouchesOnlyTheAddonsThatStrayedFromTheProfileDestination();
    static void RelinkingWhatNeverStrayedIsRefusedInsteadOfChurningTheLinks();
    static void TheSuggestionsCoverTheAddonsUnderTheClickedNodeAndUseItsOwnLibrary();
    static void ApplyingSuggestionsSendsEachAddonToItsOwnSuggestedCategory();
};

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kAircrafts = "D:/MSFS 2024/Aircrafts";
    constexpr auto kSceneries = "D:/MSFS 2024/Sceneries";
    constexpr auto kTraffic = "D:/MSFS 2024/Traffic";
    constexpr auto kTrafficAddon = "D:/MSFS 2024/Aircrafts/aig-traffic";
    constexpr auto kAddon = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";
    constexpr auto kOtherAddon = "D:/MSFS 2024/Aircrafts/aerosoft-crj";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kCommunity2024 = "E:/Flight Simulator 2024/Community2024";

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

    TreeNode DeclaredCategoryNode(const std::filesystem::path& path)
    {
        TreeNode node = CategoryNode(path, {});
        node.declaredAsCategory = true;

        return node;
    }

    TreeNode LibraryTree()
    {
        TreeNode node = CategoryNode(
            kLibrary,
            {CategoryNode(kAircrafts, {AddonNode(kAddon), AddonNode(kOtherAddon)}), DeclaredCategoryNode(kSceneries)});
        node.kind = TreeNodeKind::Library;

        return node;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity, kCommunity2024};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{"library-1", kLibrary, "MSFS 2024"}};

        return profile;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory(kCommunity2024);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kAircrafts);
            fileSystem.AddDirectory(kAddon);
            fileSystem.AddDirectory(kOtherAddon);
            catalog.SetTree(kLibrary, LibraryTree());

            settings.stored.profiles = {Profile()};
            settings.stored.activeProfileId = "msfs2024";

            session.ShowActiveProfile();
        }

        void LinkIn(const std::filesystem::path& destination, const std::filesystem::path& addonFolder)
        {
            fileSystem.AddLink(destination / addonFolder.filename(), addonFolder);
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeProcessProbe processProbe;
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        ProfileService service{catalog, classifier, linking, log, identities, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, runner, notifier};
        AddonTreeModel model;
        AddonTreeViewModel viewModel{session, service, processProbe, model, notifier};
    };
}

void AddonTreeViewModelTest::ABlankCategoryNameIsRefusedBeforeItReachesTheJournal()
{
    Fixture f;
    const TreeNode library = LibraryTree();

    const QSignalSpy refused(&f.viewModel, &AddonTreeViewModel::Refused);

    f.viewModel.CreateCategory(&library, QStringLiteral("   "));

    QCOMPARE(refused.size(), 1);
    QVERIFY(f.journal.appended.empty());
}

void AddonTreeViewModelTest::ACategoryAskedForOnAnAddonLandsBesideItInsteadOfInsideIt()
{
    Fixture f;
    const TreeNode addon = AddonNode(kAddon);

    f.viewModel.CreateCategory(&addon, QStringLiteral("Liveries"));

    QVERIFY(f.fileSystem.Exists("D:/MSFS 2024/Aircrafts/Liveries"));
    QVERIFY(!f.fileSystem.Exists("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/Liveries"));
}

void AddonTreeViewModelTest::RenamingACategoryToTheNameItAlreadyHasIsNotAFailure()
{
    Fixture f;
    const TreeNode category = CategoryNode(kAircrafts, {});

    const QSignalSpy refused(&f.viewModel, &AddonTreeViewModel::Refused);

    f.viewModel.RenameCategory(&category, QStringLiteral("Aircrafts"));

    QCOMPARE(refused.size(), 0);
    QVERIFY(f.journal.appended.empty());
    QVERIFY(f.fileSystem.Exists(kAircrafts));
}

void AddonTreeViewModelTest::ARefusedRenameIsExplainedInsteadOfPassingSilently()
{
    Fixture f;
    f.fileSystem.AddDirectory("D:/MSFS 2024/Sceneries");
    const TreeNode category = CategoryNode(kAircrafts, {});

    const QSignalSpy refused(&f.viewModel, &AddonTreeViewModel::Refused);

    f.viewModel.RenameCategory(&category, QStringLiteral("Sceneries"));

    QCOMPARE(refused.size(), 1);
    QVERIFY(!refused.front().front().toString().isEmpty());
    QVERIFY(f.fileSystem.Exists(kAircrafts));
}

void AddonTreeViewModelTest::MovingASelectionThatHoldsACategoryMovesOnlyTheAddonsInIt()
{
    Fixture f;
    f.fileSystem.AddDirectory(kSceneries);
    const TreeNode addon = AddonNode(kAddon);
    const TreeNode category = CategoryNode(kAircrafts, {});

    f.viewModel.MoveTo({&category, &addon}, kSceneries);

    QVERIFY(f.fileSystem.Exists("D:/MSFS 2024/Sceneries/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.Exists(kAircrafts));
}

void AddonTreeViewModelTest::MovingASelectionWithNoAddonInItIsRefusedBeforeItReachesTheJournal()
{
    Fixture f;
    f.fileSystem.AddDirectory(kSceneries);
    const TreeNode category = CategoryNode(kAircrafts, {});

    const QSignalSpy refused(&f.viewModel, &AddonTreeViewModel::Refused);

    f.viewModel.MoveTo({&category}, kSceneries);

    QCOMPARE(refused.size(), 1);
    QVERIFY(f.journal.appended.empty());
}

void AddonTreeViewModelTest::TheCategoryAnAddonAlreadySitsInIsNotOfferedAsAMoveTarget()
{
    Fixture f;
    const TreeNode addon = AddonNode(kAddon);

    const std::vector<MoveTarget> offered = f.viewModel.CategoriesFor(&addon);

    QCOMPARE(offered.size(), std::size_t{1});
    QCOMPARE(offered.front().category, std::filesystem::path{kSceneries});
}

void AddonTreeViewModelTest::TheLibraryRootIsNotOfferedSoAMoveNeverLandsAnAddonLoose()
{
    Fixture f;
    const TreeNode addon = AddonNode(kAddon);

    const std::vector<MoveTarget> offered = f.viewModel.CategoriesFor(&addon);

    QVERIFY(std::ranges::none_of(offered,
                                 [](const MoveTarget& target)
                                 {
                                     return target.category == std::filesystem::path{kLibrary};
                                 }));
}

void AddonTreeViewModelTest::AnEmptyFolderNobodyDeclaredIsLeftOutOfTheMoveTargets()
{
    Fixture f;
    TreeNode tree = CategoryNode(
        kLibrary,
        {CategoryNode(kAircrafts, {AddonNode(kAddon)}), DeclaredCategoryNode(kSceneries), CategoryNode(kTraffic, {})});
    tree.kind = TreeNodeKind::Library;
    f.catalog.SetTree(kLibrary, tree);
    f.session.ShowActiveProfile();

    const TreeNode addon = AddonNode(kAddon);

    const std::vector<MoveTarget> offered = f.viewModel.CategoriesFor(&addon);

    QCOMPARE(offered.size(), std::size_t{1});
    QCOMPARE(offered.front().category, std::filesystem::path{kSceneries});
}

void AddonTreeViewModelTest::ANestedCategoryIsOfferedByItsPathSoTwoOfTheSameNameStayApart()
{
    Fixture f;
    const std::filesystem::path nested = std::filesystem::path{kAircrafts} / "Liveries";
    const std::filesystem::path loose = std::filesystem::path{kLibrary} / "Liveries";

    TreeNode tree =
        CategoryNode(kLibrary,
                     {CategoryNode(kAircrafts, {AddonNode(kAddon), CategoryNode(nested, {AddonNode(nested / "one")})}),
                      CategoryNode(loose, {AddonNode(loose / "two")})});
    tree.kind = TreeNodeKind::Library;
    f.catalog.SetTree(kLibrary, tree);
    f.session.ShowActiveProfile();

    const TreeNode addon = AddonNode(kAddon);

    const std::vector<MoveTarget> offered = f.viewModel.CategoriesFor(&addon);

    QCOMPARE(offered.size(), std::size_t{2});
    QCOMPARE(offered.front().relativePath, std::filesystem::path{"Aircrafts/Liveries"});
    QCOMPARE(offered.back().relativePath, std::filesystem::path{"Liveries"});
}

void AddonTreeViewModelTest::AnAddonInALibraryWithNoCategoryHasNowhereToBeMovedTo()
{
    Fixture f;
    TreeNode tree = CategoryNode(kLibrary, {AddonNode(kAddon)});
    tree.kind = TreeNodeKind::Library;
    f.catalog.SetTree(kLibrary, tree);
    f.session.ShowActiveProfile();

    const TreeNode addon = AddonNode(kAddon);

    QVERIFY(f.viewModel.CategoriesFor(&addon).empty());
}

void AddonTreeViewModelTest::AdoptingWritesTheOverrideOnTheCategoryWhenEveryEnabledAddonAgrees()
{
    Fixture f;
    f.LinkIn(kCommunity2024, kAddon);
    f.LinkIn(kCommunity2024, kOtherAddon);
    f.session.ShowActiveProfile();

    const TreeNode category = CategoryNode(kAircrafts, {AddonNode(kAddon), AddonNode(kOtherAddon)});
    const QSignalSpy refused(&f.viewModel, &AddonTreeViewModel::Refused);

    f.viewModel.AdoptDestination(&category);

    QCOMPARE(refused.size(), 0);

    const std::vector<DestinationOverride>& written = f.settings.stored.profiles.front().destinationOverrides;
    QCOMPARE(written.size(), std::size_t{1});
    QCOMPARE(ComparablePath(written.front().relativePath), ComparablePath("Aircrafts"));
    QCOMPARE(ComparablePath(written.front().destination), ComparablePath(kCommunity2024));
}

void AddonTreeViewModelTest::AdoptingIsRefusedWhenTheEnabledAddonsPointAtDifferentDestinations()
{
    Fixture f;
    f.LinkIn(kCommunity2024, kAddon);
    f.LinkIn(kCommunity, kOtherAddon);
    f.session.ShowActiveProfile();

    const TreeNode category = CategoryNode(kAircrafts, {AddonNode(kAddon), AddonNode(kOtherAddon)});
    const QSignalSpy refused(&f.viewModel, &AddonTreeViewModel::Refused);

    f.viewModel.AdoptDestination(&category);

    QCOMPARE(refused.size(), 1);
    QVERIFY(f.settings.stored.profiles.front().destinationOverrides.empty());
}

void AddonTreeViewModelTest::AdoptingIsRefusedWhenNoAddonInTheCategoryIsEnabled()
{
    Fixture f;

    const TreeNode category = CategoryNode(kAircrafts, {AddonNode(kAddon), AddonNode(kOtherAddon)});
    const QSignalSpy refused(&f.viewModel, &AddonTreeViewModel::Refused);

    f.viewModel.AdoptDestination(&category);

    QCOMPARE(refused.size(), 1);
    QVERIFY(f.settings.stored.profiles.front().destinationOverrides.empty());
}

void AddonTreeViewModelTest::OnlyACategoryHoldingAStrayAddonIsWorthOfferingTheAdoption()
{
    Fixture f;
    const TreeNode category = CategoryNode(kAircrafts, {AddonNode(kAddon), AddonNode(kOtherAddon)});

    f.LinkIn(kCommunity, kAddon);
    f.session.ShowActiveProfile();
    QCOMPARE(f.viewModel.StrayAddonsUnder({&category}), std::size_t{0});

    f.LinkIn(kCommunity2024, kOtherAddon);
    f.session.ShowActiveProfile();
    QCOMPARE(f.viewModel.StrayAddonsUnder({&category}), std::size_t{1});
}

void AddonTreeViewModelTest::TurningAStrayAddonOffAndOnAgainLandsItInTheDestinationTheProfileMandates()
{
    Fixture f;
    const TreeNode addon = AddonNode(kAddon);

    f.viewModel.OverrideDestination({&addon}, kCommunity2024);
    f.LinkIn(kCommunity, kAddon);
    f.session.ShowActiveProfile();

    const auto strayedTo = [&f]
    {
        return DestinationItStrayedTo(f.viewModel.Profile(), f.session.Snapshot().entries, kAddon);
    };

    QCOMPARE(strayedTo(), std::filesystem::path{kCommunity});

    f.viewModel.Toggle({&addon}, false);
    f.viewModel.Toggle({&addon}, true);
    f.session.ShowActiveProfile();

    QVERIFY(!f.fileSystem.Exists(std::filesystem::path{kCommunity} / std::filesystem::path{kAddon}.filename()));
    QVERIFY(f.fileSystem.IsLink(std::filesystem::path{kCommunity2024} / std::filesystem::path{kAddon}.filename()));
    QVERIFY(strayedTo().empty());
}

void AddonTreeViewModelTest::RelinkingTouchesOnlyTheAddonsThatStrayedFromTheProfileDestination()
{
    Fixture f;
    const TreeNode category = CategoryNode(kAircrafts, {AddonNode(kAddon), AddonNode(kOtherAddon)});

    f.viewModel.OverrideDestination({&category}, kCommunity2024);
    f.LinkIn(kCommunity, kAddon);
    f.LinkIn(kCommunity2024, kOtherAddon);
    f.session.ShowActiveProfile();

    f.viewModel.RelinkToTheProfileDestination({&category});

    QVERIFY(!f.fileSystem.Exists(std::filesystem::path{kCommunity} / "pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsLink(std::filesystem::path{kCommunity2024} / "pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsLink(std::filesystem::path{kCommunity2024} / "aerosoft-crj"));

    QVERIFY(!f.journal.appended.empty());
    for (const OperationRecord& record : f.journal.appended)
    {
        QCOMPARE(record.source, std::filesystem::path{kAddon});
    }
}

void AddonTreeViewModelTest::RelinkingWhatNeverStrayedIsRefusedInsteadOfChurningTheLinks()
{
    Fixture f;
    const TreeNode category = CategoryNode(kAircrafts, {AddonNode(kAddon)});

    f.LinkIn(kCommunity, kAddon);
    f.session.ShowActiveProfile();

    const QSignalSpy refused(&f.viewModel, &AddonTreeViewModel::Refused);

    f.viewModel.RelinkToTheProfileDestination({&category});

    QCOMPARE(refused.size(), 1);
    QVERIFY(f.journal.appended.empty());
}

void AddonTreeViewModelTest::TheSuggestionsCoverTheAddonsUnderTheClickedNodeAndUseItsOwnLibrary()
{
    Fixture f;
    const TreeNode aircrafts = CategoryNode(kAircrafts, {AddonNode(kAddon), AddonNode(kTrafficAddon)});

    TreeNode tree = CategoryNode(kLibrary, {aircrafts, CategoryNode(kTraffic, {})});
    tree.kind = TreeNodeKind::Library;
    f.catalog.SetTree(kLibrary, tree);
    f.session.ShowActiveProfile();

    const std::vector<CategorySuggestion> suggestions = f.viewModel.SuggestionsFor(&aircrafts);

    QCOMPARE(suggestions.size(), std::size_t{2});

    const auto moving = std::ranges::find_if(suggestions,
                                             [](const CategorySuggestion& suggestion)
                                             {
                                                 return suggestion.WouldMove();
                                             });

    QVERIFY(moving != suggestions.end());
    QCOMPARE(moving->addonFolder, std::filesystem::path{kTrafficAddon});
    QCOMPARE(moving->suggestedCategory, std::filesystem::path{kTraffic});
    QCOMPARE(moving->rule, CategoryRule::TheNameSaysTraffic);
}

void AddonTreeViewModelTest::ApplyingSuggestionsSendsEachAddonToItsOwnSuggestedCategory()
{
    Fixture f;
    f.fileSystem.AddDirectory(kSceneries);
    f.fileSystem.AddDirectory(kTraffic);

    f.viewModel.ApplySuggestions(
        {CategorySuggestion{kAddon, kAircrafts, kSceneries, CategoryRule::TheNameSaysAirport},
         CategorySuggestion{kOtherAddon, kAircrafts, kTraffic, CategoryRule::TheNameSaysTraffic}});

    QVERIFY(f.fileSystem.Exists("D:/MSFS 2024/Sceneries/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.Exists("D:/MSFS 2024/Traffic/aerosoft-crj"));
}

QTEST_MAIN(AddonTreeViewModelTest)

#include "tst_addon_tree_view_model.moc"
