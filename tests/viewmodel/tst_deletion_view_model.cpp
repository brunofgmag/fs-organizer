#include <QtTest/QtTest>

#include <cstdint>
#include <filesystem>
#include <vector>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/support/PathUtils.h"
#include "domain/model/RecycleLimits.h"
#include "domain/tree/AddonTree.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
#include "viewmodel/SessionNotifier.h"
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
#include "viewmodel/DeletionViewModel.h"

namespace
{
    class DeletionViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheSelectionIsWeighedBeforeThePlanIsAllowedToJudgeIt();
        static void ASelectionOnTwoVolumesWherePartFitsTheBinAnswersForEachAddon();
        static void AnAddonTooDeepForTheBinIsTheOnlyOneOfTheSelectionLeftBehind();
        static void AnAddonNobodyMeasuredSaysSoInsteadOfBlamingTheBin();
        static void TheGestureIsPlannedOnceAndTheDeletionRunsAgainstThatSamePlan();
        static void AnAddonMeasuredByAnotherScreenIsWeighedAgainBeforeTheGuardsJudgeIt();
        static void ADeletionThatTookSomethingAwayForgetsTheUndoOfTheLastBatch();
        static void ADeletionThatTookNothingAwayLeavesTheUndoWhereItWas();
        static void ACategoryInTheSelectionIsCountedApartAndTakesNoAddonWithIt();
        static void AnAddonEnabledInAProfileThatIsNotTheActiveOneReachesTheScreen();
    };
}

namespace
{
    constexpr std::uintmax_t kMegabyte = 1024 * 1024;
    constexpr std::uintmax_t kGigabyte = 1024 * kMegabyte;

    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kOtherDestination = "F:/Sim2020/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kAircrafts = "D:/Library/Aircrafts";
    const std::filesystem::path kCrj = "D:/Library/Aircrafts/aerosoft-crj";
    const std::filesystem::path kAtr = "D:/Library/Aircrafts/hype-atr";
    const std::filesystem::path kArchive = "E:/Archive";
    const std::filesystem::path kMd11 = "E:/Archive/tfdidesign-md11";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    TreeNode LibraryTree()
    {
        TreeNode aircrafts;
        aircrafts.kind = TreeNodeKind::Category;
        aircrafts.path = kAircrafts;
        aircrafts.children = {AddonNode(kCrj), AddonNode(kAtr)};

        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = {std::move(aircrafts)};

        return library;
    }

    TreeNode ArchiveTree()
    {
        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kArchive;
        library.children = {AddonNode(kMd11)};

        return library;
    }

    SimulatorProfile Active()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kDestination};
        profile.defaultDestination = kDestination;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"},
                             Library{.id = "library-3", .path = kArchive, .label = "Archive"}};

        return profile;
    }

    SimulatorProfile Other()
    {
        SimulatorProfile profile;
        profile.id = "msfs2020";
        profile.variant = SimulatorVariant::MSFS2020;
        profile.destinations = {kOtherDestination};
        profile.defaultDestination = kOtherDestination;
        profile.libraries = {Library{.id = "library-2", .path = kLibrary, .label = "MSFS 2020"}};

        return profile;
    }

    AppSettings Stored()
    {
        AppSettings settings = SettingsWith(Active());
        settings.profiles.push_back(Other());

        return settings;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kDestination);
            fileSystem.AddDirectory(kOtherDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kAircrafts);
            fileSystem.AddDirectory(kCrj);
            fileSystem.AddDirectory(kAtr);
            fileSystem.AddFile(kCrj / "manifest.json", kMegabyte);
            fileSystem.AddFile(kAtr / "manifest.json", kMegabyte);
            fileSystem.AddDirectory(kArchive);
            fileSystem.AddDirectory(kMd11);
            fileSystem.AddFile(kMd11 / "manifest.json", kMegabyte);
            fileSystem.SetRecycleBinQuota("D:", 10 * kGigabyte);
            fileSystem.SetRecycleBinQuota("E:", 10 * kGigabyte);

            catalog.SetTree(kLibrary, LibraryTree());
            catalog.SetTree(kArchive, ArchiveTree());

            session.ShowActiveProfile();
        }

        [[nodiscard]] const TreeNode* Node(const std::filesystem::path& folder) const
        {
            return AddonAt(session.Snapshot().libraries, folder);
        }

        [[nodiscard]] const TreeNode* Category() const
        {
            return session.Snapshot().libraries.front().children.data();
        }

        InMemoryFileSystem fileSystem;
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeLinkService linkService{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeProcessProbe processProbe;
        FakeLibraryIdGenerator identities;
        EntryClassifier classifier{linkService, filesystemProbe};
        LinkingEngine linking{linkService, filesystemProbe};
        StartupOverFakes startup{filesystemProbe};

        ProfileService profiles{catalog, filesystemProbe, sidecars,        classifier,        linking,
                                log,     identities,      startup.service, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings{Stored()};
        InlineBackgroundRunner runner;
        SessionNotifier notifier{};
        Session session{profiles, organizer, settings, settings.stored, processProbe, runner, notifier};
        SizeService sizes{catalog, filesystemProbe, clock, runner};
        DeletionService service{filesystemProbe, files, sidecars, linking, classifier, processProbe, log, sizes};
        DeletionViewModel viewModel{session, profiles, service, sizes};
    };

    DeletionPlan LastPlan(const QSignalSpy& spy)
    {
        return spy.isEmpty() ? DeletionPlan{} : spy.back().front().value<DeletionPlan>();
    }

    std::vector<DeletionResult> LastResults(const QSignalSpy& spy)
    {
        return spy.isEmpty() ? std::vector<DeletionResult>{} : spy.back().front().value<std::vector<DeletionResult>>();
    }

    DeletionPlan PlanFor(DeletionViewModel& viewModel, const std::vector<const TreeNode*>& nodes)
    {
        const QSignalSpy planned(&viewModel, &DeletionViewModel::Planned);
        viewModel.PlanToDelete(nodes);

        return LastPlan(planned);
    }
}

void DeletionViewModelTest::TheSelectionIsWeighedBeforeThePlanIsAllowedToJudgeIt()
{
    Fixture f;

    const DeletionPlan plan = PlanFor(f.viewModel, {f.Node(kCrj)});

    QCOMPARE(plan.addons.size(), std::size_t{1});
    QCOMPARE(plan.addons.front().bytes, std::optional<std::uintmax_t>{kMegabyte});
    QVERIFY(plan.addons.front().longestEntry.has_value());
    QCOMPARE(f.filesystemProbe.TimesWalked(kCrj), std::size_t{1});
}

void DeletionViewModelTest::ASelectionOnTwoVolumesWherePartFitsTheBinAnswersForEachAddon()
{
    Fixture f;
    f.fileSystem.AddFile(kMd11 / "scenery.bin", 20 * kGigabyte);

    const DeletionPlan plan = PlanFor(f.viewModel, {f.Node(kCrj), f.Node(kMd11)});

    QCOMPARE(plan.addons.size(), std::size_t{2});
    QCOMPARE(AddonsTheRecycleBinRefuses(plan), std::size_t{1});
    QCOMPARE(WhatTheRecycleBinRefuses(plan, plan.addons.front()), FileResult::Completed);
    QCOMPARE(WhatTheRecycleBinRefuses(plan, plan.addons.back()), FileResult::TheRecycleBinIsTooSmall);

    const QSignalSpy deleted(&f.viewModel, &DeletionViewModel::Deleted);
    f.viewModel.Delete(plan, DeletionRoute::RecycleBin);

    const std::vector<DeletionResult> results = LastResults(deleted);

    QCOMPARE(results.size(), std::size_t{2});
    QCOMPARE(results.front().result, FileResult::Completed);
    QCOMPARE(results.back().result, FileResult::TheRecycleBinIsTooSmall);
    QVERIFY(!f.fileSystem.Exists(kCrj));
    QVERIFY(f.fileSystem.Exists(kMd11));
}

void DeletionViewModelTest::AnAddonTooDeepForTheBinIsTheOnlyOneOfTheSelectionLeftBehind()
{
    Fixture f;
    const std::string buried(kTheRecycleBinStopsAt - ComparablePath(kAtr).size() - 1, 'x');
    f.fileSystem.AddFile(kAtr / PathFromUtf8(buried), kMegabyte);

    const DeletionPlan plan = PlanFor(f.viewModel, {f.Node(kCrj), f.Node(kAtr)});

    QCOMPARE(AddonsTheRecycleBinRefuses(plan), std::size_t{1});
    QCOMPARE(WhatTheRecycleBinRefuses(plan, plan.addons.back()), FileResult::TheRecycleBinCannotReachIt);

    const QSignalSpy deleted(&f.viewModel, &DeletionViewModel::Deleted);
    f.viewModel.Delete(plan, DeletionRoute::RecycleBin);

    const std::vector<DeletionResult> results = LastResults(deleted);

    QCOMPARE(results.front().result, FileResult::Completed);
    QCOMPARE(results.back().result, FileResult::TheRecycleBinCannotReachIt);
    QVERIFY(!f.fileSystem.Exists(kCrj));
    QVERIFY(f.fileSystem.Exists(kAtr));
}

void DeletionViewModelTest::AnAddonNobodyMeasuredSaysSoInsteadOfBlamingTheBin()
{
    const DeletionPlan unmeasured{.addons = {AddonToDelete{.folder = kCrj}}};

    QCOMPARE(WhatTheRecycleBinRefuses(unmeasured, unmeasured.addons.front()), FileResult::TheAddonWasNeverMeasured);

    const DeletionPlan measured{.addons = {AddonToDelete{.folder = kCrj, .bytes = kMegabyte}}};

    QCOMPARE(WhatTheRecycleBinRefuses(measured, measured.addons.front()), FileResult::TheRecycleBinIsTooSmall);
}

void DeletionViewModelTest::TheGestureIsPlannedOnceAndTheDeletionRunsAgainstThatSamePlan()
{
    Fixture f;

    const DeletionPlan plan = PlanFor(f.viewModel, {f.Node(kCrj)});

    QCOMPARE(f.filesystemProbe.TimesWalked(kCrj), std::size_t{1});
    QCOMPARE(f.filesystemProbe.TimesTheRecycleBinWasAsked(), std::size_t{1});

    f.viewModel.Delete(plan, DeletionRoute::RecycleBin);

    QCOMPARE(f.filesystemProbe.TimesWalked(kCrj), std::size_t{1});
    QCOMPARE(f.filesystemProbe.TimesTheRecycleBinWasAsked(), std::size_t{1});
    QVERIFY(!f.fileSystem.Exists(kCrj));
}

void DeletionViewModelTest::AnAddonMeasuredByAnotherScreenIsWeighedAgainBeforeTheGuardsJudgeIt()
{
    Fixture f;
    f.sizes.MeasureFolders({kCrj}, f.sizes.NewCaller(), Freshness::ReuseWhatIsKnown, {}, {});
    f.fileSystem.AddFile(kCrj / "scenery.bin", 20 * kGigabyte);

    const DeletionPlan plan = PlanFor(f.viewModel, {f.Node(kCrj)});

    QCOMPARE(plan.addons.front().bytes, std::optional<std::uintmax_t>{kMegabyte + 20 * kGigabyte});
    QVERIFY(!TheRecycleBinCanTake(plan));
}

void DeletionViewModelTest::ADeletionThatTookSomethingAwayForgetsTheUndoOfTheLastBatch()
{
    Fixture f;
    static_cast<void>(f.profiles.SetEnabled(f.session.Profile(), f.session.Snapshot(), {f.Node(kAtr)}, true));
    QVERIFY(f.profiles.CanUndo());

    f.viewModel.Delete(PlanFor(f.viewModel, {f.Node(kCrj)}), DeletionRoute::Permanently);

    QVERIFY(!f.profiles.CanUndo());
}

void DeletionViewModelTest::ADeletionThatTookNothingAwayLeavesTheUndoWhereItWas()
{
    Fixture f;
    static_cast<void>(f.profiles.SetEnabled(f.session.Profile(), f.session.Snapshot(), {f.Node(kAtr)}, true));
    QVERIFY(f.profiles.CanUndo());

    const DeletionPlan plan = PlanFor(f.viewModel, {f.Node(kCrj)});
    f.files.MakeTheRemovalFailFor(kCrj);

    f.viewModel.Delete(plan, DeletionRoute::Permanently);

    QVERIFY(f.fileSystem.Exists(kCrj));
    QVERIFY(f.profiles.CanUndo());
}

void DeletionViewModelTest::ACategoryInTheSelectionIsCountedApartAndTakesNoAddonWithIt()
{
    Fixture f;

    const DeletionPlan plan = PlanFor(f.viewModel, {f.Category()});

    QCOMPARE(plan.addons.size(), std::size_t{0});
    QCOMPARE(plan.nodesThatAreNotAddons, std::size_t{1});

    f.viewModel.Delete(plan, DeletionRoute::Permanently);

    QVERIFY(f.fileSystem.Exists(kCrj));
    QVERIFY(f.fileSystem.Exists(kAtr));
}

void DeletionViewModelTest::AnAddonEnabledInAProfileThatIsNotTheActiveOneReachesTheScreen()
{
    Fixture f;
    f.fileSystem.AddLink(kOtherDestination / "aerosoft-crj", kCrj);

    const DeletionPlan plan = PlanFor(f.viewModel, {f.Node(kCrj)});

    QCOMPARE(plan.addons.front().enabled.size(), std::size_t{1});
    QCOMPARE(plan.addons.front().enabled.front().profileId, std::string{"msfs2020"});
}

QTEST_MAIN(DeletionViewModelTest)

#include "tst_deletion_view_model.moc"
