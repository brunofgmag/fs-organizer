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
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/FakeSimulatorPackages.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/AddonTreeViewModel.h"

namespace
{
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
        static void OneUndoAfterARelinkPutsTheStrayLinkBackInsteadOfLeavingTheAddonOff();
        static void RenamingACategoryPutsTheUndoOutOfReachInsteadOfLettingItBreakALink();
        static void TheSuggestionsCoverTheAddonsUnderTheClickedNodeAndUseItsOwnLibrary();
        static void ApplyingSuggestionsSendsEachAddonToItsOwnSuggestedCategory();
        static void ACategoryCountsOnlyTheAddonsThatWouldReallyChangeState();
        static void TheSimulatorListIsNotConsultedWhileTheTreeIsBeingScanned();
        static void OnlyAnAddonHasDependenciesToReport();
        static void TheSizeOfTheSelectionIsMeasuredInTheBackgroundAndAnnouncedWhenItLands();
        static void AMeasurementOvertakenByANewSelectionIsNeverShown();
        static void AnAddonTheLibraryWalkAlreadyMeasuredIsNotWalkedAgain();
        static void ThePlaceTakenByAnAddonOfYoursIsOfferedAsASwapAndNothingElseIs();
        static void AnAgreedSwapTurnsOneOffAndTheOtherOnInASingleBatch();
        static void ARefusedSwapWritesNothingAndSaysTheAddonWasLeftAsItIs();
        static void ARefusedSwapStillTurnsOnTheRestOfTheSelection();
        static void ASwapAgreedOnAPairThatChangedOnTheDiskIsNotCarriedOut();
        static void TheToggleBatchRunsInAWorkerAndASecondGestureWaitsItsTurn();
    };
}

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
    constexpr auto kSpare = "F:/Spare";
    constexpr auto kSpareAircrafts = "F:/Spare/Aircrafts";
    constexpr auto kSpareRival = "F:/Spare/Aircrafts/pmdg-aircraft-77w";
    constexpr auto kSpareLoner = "F:/Spare/Aircrafts/fenix-a320";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    TreeNode AddonNodeDeclaring(const std::filesystem::path& path, std::vector<DeclaredDependency> dependencies)
    {
        TreeNode node = AddonNode(path);
        node.addon->manifest.dependencies = std::move(dependencies);

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
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

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

            session.ShowActiveProfile();
        }

        void LinkIn(const std::filesystem::path& destination, const std::filesystem::path& addonFolder)
        {
            fileSystem.AddLink(destination / addonFolder.filename(), addonFolder);
        }

        void RegisterTheSpareLibrary()
        {
            fileSystem.AddDirectory(kSpare);
            fileSystem.AddDirectory(kSpareAircrafts);
            fileSystem.AddDirectory(kSpareRival);
            fileSystem.AddDirectory(kSpareLoner);

            TreeNode spare =
                CategoryNode(kSpare, {CategoryNode(kSpareAircrafts, {AddonNode(kSpareRival), AddonNode(kSpareLoner)})});
            spare.kind = TreeNodeKind::Library;
            catalog.SetTree(kSpare, std::move(spare));

            QVERIFY(session.RegisterLibrary(kSpare).Accepted());

            journal.appended.clear();
        }

        [[nodiscard]] const TreeNode* SpareCategory() const
        {
            return &session.Snapshot().libraries.back().children.front();
        }

        [[nodiscard]] const TreeNode* SpareAddon(const std::size_t index) const
        {
            return &session.Snapshot().libraries.back().children.front().children[index];
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeProcessProbe processProbe;
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        StartupOverFakes startup{filesystemProbe};

        ProfileService service{catalog, filesystemProbe, sidecars,        classifier,        linking,
                               log,     identities,      startup.service, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings{SettingsWith(Profile())};
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, settings.stored, processProbe, runner, notifier};
        AddonTreeModel model;
        FakeSimulatorPackages packages;
        SizeService sizes{catalog, filesystemProbe, clock, runner};
        AddonTreeViewModel viewModel{session, service, model, packages, sizes, runner, notifier};
    };

    SelectionSize LastSize(const QSignalSpy& measured)
    {
        return measured.isEmpty() ? SelectionSize{} : measured.back().front().value<SelectionSize>();
    }
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

    QCOMPARE(f.viewModel.RenameCategory(&category, QStringLiteral("Aircrafts")), std::filesystem::path{kAircrafts});

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

    QVERIFY(f.viewModel.RenameCategory(&category, QStringLiteral("Sceneries")).empty());

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
    const Fixture f;
    const TreeNode addon = AddonNode(kAddon);

    const std::vector<MoveTarget> offered = f.viewModel.CategoriesFor(&addon);

    QCOMPARE(offered.size(), std::size_t{1});
    QCOMPARE(offered.front().category, std::filesystem::path{kSceneries});
}

void AddonTreeViewModelTest::TheLibraryRootIsNotOfferedSoAMoveNeverLandsAnAddonLoose()
{
    const Fixture f;
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

void AddonTreeViewModelTest::ACategoryCountsOnlyTheAddonsThatWouldReallyChangeState()
{
    Fixture f;
    const TreeNode category = CategoryNode(kAircrafts, {AddonNode(kAddon), AddonNode(kOtherAddon)});

    QVERIFY(f.viewModel.WouldEnable({&category}));
    QCOMPARE(f.viewModel.AddonsThatWouldChange({&category}, true), std::size_t{2});
    QCOMPARE(f.viewModel.AddonsThatWouldChange({&category}, false), std::size_t{0});

    f.LinkIn(kCommunity, kAddon);
    f.session.ShowActiveProfile();

    QVERIFY(f.viewModel.WouldEnable({&category}));
    QCOMPARE(f.viewModel.AddonsThatWouldChange({&category}, true), std::size_t{1});
    QCOMPARE(f.viewModel.AddonsThatWouldChange({&category}, false), std::size_t{1});

    f.LinkIn(kCommunity, kOtherAddon);
    f.session.ShowActiveProfile();

    QVERIFY(!f.viewModel.WouldEnable({&category}));
    QCOMPARE(f.viewModel.AddonsThatWouldChange({&category}, false), std::size_t{2});
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

void AddonTreeViewModelTest::OneUndoAfterARelinkPutsTheStrayLinkBackInsteadOfLeavingTheAddonOff()
{
    Fixture f;
    const TreeNode category = CategoryNode(kAircrafts, {AddonNode(kAddon)});

    f.viewModel.OverrideDestination({&category}, kCommunity2024);
    f.LinkIn(kCommunity, kAddon);
    f.session.ShowActiveProfile();

    f.viewModel.RelinkToTheProfileDestination({&category});

    QVERIFY(f.viewModel.CanUndo());
    QVERIFY(f.fileSystem.IsLink(std::filesystem::path{kCommunity2024} / "pmdg-aircraft-77w"));

    f.viewModel.UndoLastBatch();

    QVERIFY(f.fileSystem.IsLink(std::filesystem::path{kCommunity} / "pmdg-aircraft-77w"));
    QVERIFY(!f.fileSystem.Exists(std::filesystem::path{kCommunity2024} / "pmdg-aircraft-77w"));
}

void AddonTreeViewModelTest::RenamingACategoryPutsTheUndoOutOfReachInsteadOfLettingItBreakALink()
{
    Fixture f;
    const TreeNode category = CategoryNode(kAircrafts, {AddonNode(kAddon)});

    f.viewModel.Toggle({&category}, true);

    QVERIFY(f.viewModel.CanUndo());

    QVERIFY(!f.viewModel.RenameCategory(&category, "Aeronaves").empty());

    QVERIFY(!f.viewModel.CanUndo());
}

void AddonTreeViewModelTest::TheSuggestionsCoverTheAddonsUnderTheClickedNodeAndUseItsOwnLibrary()
{
    Fixture f;
    const TreeNode aircrafts = CategoryNode(kAircrafts, {AddonNode(kAddon), AddonNode(kTrafficAddon)});

    TreeNode tree = CategoryNode(kLibrary, {aircrafts, DeclaredCategoryNode(kTraffic)});
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

    f.viewModel.ApplySuggestions({CategorySuggestion{.addonFolder = kAddon,
                                                     .currentCategory = kAircrafts,
                                                     .suggestedCategory = kSceneries,
                                                     .rule = CategoryRule::TheNameSaysAirport},
                                  CategorySuggestion{.addonFolder = kOtherAddon,
                                                     .currentCategory = kAircrafts,
                                                     .suggestedCategory = kTraffic,
                                                     .rule = CategoryRule::TheNameSaysTraffic}});

    QVERIFY(f.fileSystem.Exists("D:/MSFS 2024/Sceneries/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.Exists("D:/MSFS 2024/Traffic/aerosoft-crj"));
}

void AddonTreeViewModelTest::TheSimulatorListIsNotConsultedWhileTheTreeIsBeingScanned()
{
    Fixture f;
    const TreeNode declaring = AddonNodeDeclaring(kAddon, {{"fs-base-ui", "0.1.10"}});
    f.catalog.SetTree(kLibrary, CategoryNode(kLibrary, {CategoryNode(kAircrafts, {declaring})}));
    f.session.RefreshEntries();
    f.viewModel.ShowActiveProfile();

    QCOMPARE(f.packages.asked, std::size_t{0});

    const DependencyReport report = f.viewModel.DependenciesOf(&declaring);

    QCOMPARE(report.answers.size(), std::size_t{1});
    QCOMPARE(f.packages.asked, std::size_t{1});
}

void AddonTreeViewModelTest::OnlyAnAddonHasDependenciesToReport()
{
    Fixture f;
    const TreeNode category = CategoryNode(kAircrafts, {});

    QVERIFY(f.viewModel.DependenciesOf(nullptr).answers.empty());
    QVERIFY(f.viewModel.DependenciesOf(&category).answers.empty());
    QCOMPARE(f.packages.asked, std::size_t{0});
}

void AddonTreeViewModelTest::TheSizeOfTheSelectionIsMeasuredInTheBackgroundAndAnnouncedWhenItLands()
{
    Fixture f;
    f.fileSystem.AddFile(std::filesystem::path(kAddon) / "content.bin", 300);
    f.fileSystem.AddFile(std::filesystem::path(kOtherAddon) / "content.bin", 700);
    f.runner.defer = true;

    const QSignalSpy measuring(&f.viewModel, &AddonTreeViewModel::SizeMeasuring);
    const QSignalSpy measured(&f.viewModel, &AddonTreeViewModel::SizeMeasured);

    f.viewModel.MeasureTheSelection({kAddon, kOtherAddon});

    QCOMPARE(measuring.size(), 1);
    QCOMPARE(measured.size(), 0);

    f.runner.Finish();

    QCOMPARE(measured.size(), 1);
    QCOMPARE(LastSize(measured).bytes, std::uintmax_t{1000});
    QCOMPARE(LastSize(measured).measured, std::size_t{2});
    QCOMPARE(LastSize(measured).selected, std::size_t{2});
}

void AddonTreeViewModelTest::AMeasurementOvertakenByANewSelectionIsNeverShown()
{
    Fixture f;
    f.fileSystem.AddFile(std::filesystem::path(kAddon) / "content.bin", 300);
    f.fileSystem.AddFile(std::filesystem::path(kOtherAddon) / "content.bin", 700);
    f.runner.defer = true;

    const QSignalSpy measured(&f.viewModel, &AddonTreeViewModel::SizeMeasured);

    f.viewModel.MeasureTheSelection({kAddon, kOtherAddon});
    f.viewModel.MeasureTheSelection({kOtherAddon});

    f.runner.Finish();
    QCOMPARE(measured.size(), 0);

    f.runner.Finish();
    QCOMPARE(measured.size(), 1);
    QCOMPARE(LastSize(measured).bytes, std::uintmax_t{700});
}

void AddonTreeViewModelTest::AnAddonTheLibraryWalkAlreadyMeasuredIsNotWalkedAgain()
{
    Fixture f;
    f.fileSystem.AddFile(std::filesystem::path(kAddon) / "content.bin", 300);

    const MeasurementCaller elsewhere = f.sizes.NewCaller();
    f.sizes.Measure({kLibrary}, elsewhere, Freshness::ReuseWhatIsKnown, {}, {});
    QCOMPARE(f.filesystemProbe.TimesWalked(kAddon), std::size_t{1});

    const QSignalSpy measured(&f.viewModel, &AddonTreeViewModel::SizeMeasured);

    f.viewModel.MeasureTheSelection({kAddon});

    QCOMPARE(measured.size(), 1);
    QCOMPARE(LastSize(measured).bytes, std::uintmax_t{300});
    QCOMPARE(f.filesystemProbe.TimesWalked(kAddon), std::size_t{1});
}

void AddonTreeViewModelTest::ThePlaceTakenByAnAddonOfYoursIsOfferedAsASwapAndNothingElseIs()
{
    Fixture f;
    f.LinkIn(kCommunity, kAddon);
    f.RegisterTheSpareLibrary();

    const std::vector<TakenPlace> swaps = f.viewModel.SwapsNeededTo({f.SpareCategory()});

    QCOMPARE(swaps.size(), std::size_t{1});
    QCOMPARE(swaps.front().addonFolder, std::filesystem::path{kSpareRival});
    QCOMPARE(swaps.front().occupant, std::filesystem::path{kAddon});
    QCOMPARE(swaps.front().linkPath, std::filesystem::path{"E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"});

    QVERIFY(f.viewModel.SwapsNeededTo({f.SpareAddon(1)}).empty());
}

void AddonTreeViewModelTest::AnAgreedSwapTurnsOneOffAndTheOtherOnInASingleBatch()
{
    Fixture f;
    const std::filesystem::path place = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w";

    f.LinkIn(kCommunity, kAddon);
    f.RegisterTheSpareLibrary();

    const QSignalSpy finished(&f.viewModel, &AddonTreeViewModel::BatchFinished);
    const std::vector<TakenPlace> swaps = f.viewModel.SwapsNeededTo({f.SpareAddon(0)});

    f.viewModel.Toggle({f.SpareAddon(0)}, true, swaps);

    QCOMPARE(finished.size(), 1);
    QCOMPARE(f.fileSystem.LinkTarget(place), std::optional<std::filesystem::path>{kSpareRival});
    QCOMPARE(f.journal.appended.size(), std::size_t{2});
    QCOMPARE(f.journal.appended.front().kind, OperationKind::DisableAddon);
    QCOMPARE(f.journal.appended.back().kind, OperationKind::EnableAddon);

    QVERIFY(f.viewModel.CanUndo());
    f.viewModel.UndoLastBatch();

    QCOMPARE(f.fileSystem.LinkTarget(place), std::optional<std::filesystem::path>{kAddon});
}

void AddonTreeViewModelTest::ARefusedSwapWritesNothingAndSaysTheAddonWasLeftAsItIs()
{
    Fixture f;
    const std::filesystem::path place = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w";

    f.LinkIn(kCommunity, kAddon);
    f.RegisterTheSpareLibrary();

    const QSignalSpy finished(&f.viewModel, &AddonTreeViewModel::BatchFinished);

    f.viewModel.Toggle({f.SpareAddon(0)}, true, {});

    QCOMPARE(finished.size(), 1);

    const auto report = finished.front().front().value<LinkBatchReport>();
    QVERIFY(report.results.empty());
    QCOMPARE(report.leftAlone, std::size_t{1});
    QVERIFY(f.journal.appended.empty());
    QCOMPARE(f.fileSystem.LinkTarget(place), std::optional<std::filesystem::path>{kAddon});
}

void AddonTreeViewModelTest::ARefusedSwapStillTurnsOnTheRestOfTheSelection()
{
    Fixture f;
    f.LinkIn(kCommunity, kAddon);
    f.RegisterTheSpareLibrary();

    const QSignalSpy finished(&f.viewModel, &AddonTreeViewModel::BatchFinished);

    f.viewModel.Toggle({f.SpareCategory()}, true, {});

    const auto report = finished.front().front().value<LinkBatchReport>();
    QCOMPARE(report.results.size(), std::size_t{1});
    QCOMPARE(report.results.front().addonFolder, std::filesystem::path{kSpareLoner});
    QCOMPARE(report.leftAlone, std::size_t{1});
    QCOMPARE(f.fileSystem.LinkTarget("E:/Flight Simulator 2024/Community/fenix-a320"),
             std::optional<std::filesystem::path>{kSpareLoner});
    QCOMPARE(f.fileSystem.LinkTarget("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"),
             std::optional<std::filesystem::path>{kAddon});
}

void AddonTreeViewModelTest::ASwapAgreedOnAPairThatChangedOnTheDiskIsNotCarriedOut()
{
    Fixture f;
    const std::filesystem::path place = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w";

    f.LinkIn(kCommunity, kAddon);
    f.RegisterTheSpareLibrary();

    const std::vector<TakenPlace> swaps = f.viewModel.SwapsNeededTo({f.SpareAddon(0)});
    QCOMPARE(swaps.size(), std::size_t{1});

    QVERIFY(f.fileSystem.RemoveNode(place));
    f.fileSystem.AddLink(place, kOtherAddon);

    const QSignalSpy finished(&f.viewModel, &AddonTreeViewModel::BatchFinished);

    f.viewModel.Toggle({f.SpareAddon(0)}, true, swaps);

    const auto report = finished.front().front().value<LinkBatchReport>();
    QVERIFY(report.results.empty());
    QCOMPARE(report.leftAlone, std::size_t{1});
    QVERIFY(f.journal.appended.empty());
    QCOMPARE(f.fileSystem.LinkTarget(place), std::optional<std::filesystem::path>{kOtherAddon});
}

void AddonTreeViewModelTest::TheToggleBatchRunsInAWorkerAndASecondGestureWaitsItsTurn()
{
    Fixture f;
    const TreeNode addon = AddonNode(kAddon);
    const std::filesystem::path place = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w";

    const QSignalSpy finished(&f.viewModel, &AddonTreeViewModel::BatchFinished);

    f.runner.defer = true;

    f.viewModel.Toggle({&addon}, true);

    QVERIFY2(f.runner.Pending(), "the batch goes through the runner instead of holding the calling thread");
    QVERIFY(f.journal.appended.empty());
    QCOMPARE(finished.size(), 0);

    f.viewModel.Toggle({&addon}, true);

    QCOMPARE(f.runner.HowManyPending(), std::size_t{1});

    f.runner.Finish();

    QCOMPARE(finished.size(), 1);
    QCOMPARE(f.fileSystem.LinkTarget(place), std::optional<std::filesystem::path>{kAddon});
    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(f.journal.appended.front().kind, OperationKind::EnableAddon);
}

QTEST_MAIN(AddonTreeViewModelTest)

#include "tst_addon_tree_view_model.moc"
