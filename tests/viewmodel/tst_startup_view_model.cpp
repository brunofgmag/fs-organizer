#include <QtTest/QtTest>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/model/Preset.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakePresetRepository.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/FakeStartupEntries.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/SessionNotifier.h"
#include "viewmodel/StartupViewModel.h"

namespace
{
    class StartupViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheScreenShowsWhatTheServiceReadAndStampsWhenItReadIt();
        static void TheLineOfAnAddonThatIsOffNowCarriesTheConditionOfTheActiveProfile();
        static void WithTheEntriesLeftLooseTheScreenShowsNothingAndNoMomentOfReading();
        static void TakingTheEntriesBackIsWrittenDownAndTheFileIsReadAgain();
        static void LeavingTheEntriesLooseIsWrittenDownAndTheFileStopsBeingRead();
        static void AChoiceTheDiskRefusesLeavesTheOptionWhereItWas();
        static void TurningAnEntryOffRereadsTheFileSoTheLineSaysWhatTheDiskSays();
        static void WithTheSimulatorOpenTheSwitchIsRefusedAndTheProcessIsNamed();
        static void TurningOnAnEntryWhoseProgramIsGoneIsWrittenAndItsConditionStaysBroken();

        static void AddingAnEntryWritesItAndReadsTheLinesTheSnapshotAndTheUndoPlanAgain();
        static void AddingAProgramInsideAnAddonWritesThePathOfTheLink();
        static void AnAddingTheEditorRefusesChangesNothingAndSaysWhy();
        static void RemovingKeepsTheEntryInTheRemovedListAndOffersTheUndo();
        static void RestoringBringsTheEntryBackAndEmptiesTheRemovedList();
        static void DiscardingForgetsTheRemovedEntryAndLeavesTheFileAlone();
        static void EditingRewritesTheLineAndAPathChangeCarriesThePresets();
        static void APresetThatCouldNotFollowAnEditIsNamedInTheOutcome();
        static void UndoingGoesThroughTheEditorAndReadsAgain();
        static void WithNothingToUndoThereIsNoOutcomeAndNoChange();
        static void TheSimulatorOpenRefusesEveryGestureAndChangesNothing();
        static void ARefusalBecauseTheDiskDisagreesRereadsSoThePhantomLineGoes();
        static void SwitchingLeavesALineInTheJournalAndUpdatesTheSnapshotOfTheSession();
        static void TheCheckOfAFileComesFromTheEditorAndWritesNothing();
        static void WithTheEntriesLeftLooseThereIsNothingToUndoAndNothingRemoved();
    };

    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kFlowInTheLibrary = "D:/Library/Utilities/p42-util-flow-pro";
    const std::filesystem::path kFlowExecutable = "E:/Sim/Community/p42-util-flow-pro/bin/flow.exe";
    const std::filesystem::path kSimlink = "C:/Program Files/Navigraph/Simlink/simlink.exe";
    const std::filesystem::path kAny2Gsx = "C:/Tools/Any2GSX/Any2GSX.exe";
    const std::filesystem::path kFlowTool = kFlowInTheLibrary / "bin" / "tool.exe";
    const std::filesystem::path kFlowToolByTheLink = kDestination / "p42-util-flow-pro" / "bin" / "tool.exe";

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
        TreeNode utilities;
        utilities.kind = TreeNodeKind::Category;
        utilities.path = "D:/Library/Utilities";
        utilities.children = {AddonNode(kFlowInTheLibrary)};

        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = {std::move(utilities)};

        return library;
    }

    SimulatorProfile Active()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kDestination};
        profile.defaultDestination = kDestination;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    AppSettings Stored(const bool managing)
    {
        AppSettings settings = SettingsWith(Active());
        settings.manageStartupEntries = managing;

        return settings;
    }

    struct Fixture
    {
        explicit Fixture(const bool managing = true) : settings(Stored(managing))
        {
            fileSystem.AddDirectory(kDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory("D:/Library/Utilities");
            fileSystem.AddDirectory(kFlowInTheLibrary);
            fileSystem.AddFile(kFlowInTheLibrary / "manifest.json");
            fileSystem.AddFile(kSimlink);
            fileSystem.AddFile(kAny2Gsx);
            fileSystem.AddFile(kFlowTool);

            catalog.SetTree(kLibrary, LibraryTree());

            entries.Carry(StartupEntry{.label = "FlowPro", .path = kFlowExecutable, .enabled = true});
            entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink, .enabled = true});

            service.Manage(managing);
            session.ShowActiveProfile();
            entries.reads = 0;
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
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier{};
        Session session{profiles, organizer, settings, settings.stored, processProbe, runner, notifier};
        FakeStartupEntries& entries = startup.entries;
        StartupService service{entries, processProbe, filesystemProbe, true};
        FakePresetRepository presets;
        StartupEditor editor{service, presets, filesystemProbe, log};
        StartupViewModel viewModel{service, editor, session, clock};
    };
}

void StartupViewModelTest::TheScreenShowsWhatTheServiceReadAndStampsWhenItReadIt()
{
    Fixture fixture;

    const QSignalSpy changed(&fixture.viewModel, &StartupViewModel::Changed);
    fixture.viewModel.Show();

    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{2});
    QCOMPARE(fixture.viewModel.Lines().front().label, std::string("FlowPro"));
    QCOMPARE(fixture.viewModel.ReadAt(), fixture.clock.now);
    QCOMPARE(changed.count(), 1);
}

void StartupViewModelTest::TheLineOfAnAddonThatIsOffNowCarriesTheConditionOfTheActiveProfile()
{
    Fixture fixture;
    fixture.viewModel.Show();

    QCOMPARE(fixture.viewModel.Lines().front().reach, StartupReach::InsideAnAddon);
    QCOMPARE(fixture.viewModel.Lines().front().condition, StartupCondition::BehindADisabledAddon);
    QCOMPARE(fixture.viewModel.Lines().front().addonFolder, kDestination / "p42-util-flow-pro");
    QCOMPARE(fixture.viewModel.Lines().back().condition, StartupCondition::Reachable);
}

void StartupViewModelTest::WithTheEntriesLeftLooseTheScreenShowsNothingAndNoMomentOfReading()
{
    Fixture fixture(false);

    fixture.viewModel.Show();

    QVERIFY(!fixture.viewModel.Managing());
    QVERIFY(fixture.viewModel.Lines().empty());
    QVERIFY(!fixture.viewModel.ReadAt().has_value());
    QCOMPARE(fixture.entries.reads, std::size_t{0});
}

void StartupViewModelTest::TakingTheEntriesBackIsWrittenDownAndTheFileIsReadAgain()
{
    Fixture fixture(false);
    fixture.viewModel.Show();

    const QSignalSpy changed(&fixture.viewModel, &StartupViewModel::Changed);
    fixture.viewModel.Manage(true);

    QVERIFY(fixture.viewModel.Managing());
    QVERIFY(fixture.settings.stored.manageStartupEntries);
    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{2});
    QVERIFY(fixture.viewModel.ReadAt().has_value());
    QCOMPARE(changed.count(), 1);
}

void StartupViewModelTest::LeavingTheEntriesLooseIsWrittenDownAndTheFileStopsBeingRead()
{
    Fixture fixture;
    fixture.viewModel.Show();

    fixture.entries.reads = 0;
    fixture.viewModel.Manage(false);

    QVERIFY(!fixture.viewModel.Managing());
    QVERIFY(!fixture.settings.stored.manageStartupEntries);
    QVERIFY(fixture.viewModel.Lines().empty());
    QVERIFY(!fixture.viewModel.ReadAt().has_value());
    QCOMPARE(fixture.entries.reads, std::size_t{0});
}

void StartupViewModelTest::AChoiceTheDiskRefusesLeavesTheOptionWhereItWas()
{
    Fixture fixture;
    fixture.viewModel.Show();
    fixture.settings.refusing = true;

    const QSignalSpy refused(&fixture.viewModel, &StartupViewModel::SettingsCouldNotBeSaved);
    fixture.viewModel.Manage(false);

    QCOMPARE(refused.count(), 1);
    QVERIFY(fixture.viewModel.Managing());
    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{2});
}

void StartupViewModelTest::TurningAnEntryOffRereadsTheFileSoTheLineSaysWhatTheDiskSays()
{
    Fixture fixture;
    fixture.viewModel.Show();

    const QSignalSpy changed(&fixture.viewModel, &StartupViewModel::Changed);

    QCOMPARE(fixture.viewModel.Switch(kFlowExecutable, false), FileResult::Completed);
    QCOMPARE(fixture.entries.writes, std::size_t{1});
    QVERIFY(!fixture.viewModel.Lines().front().enabled);
    QCOMPARE(fixture.viewModel.Lines().front().condition, StartupCondition::BehindADisabledAddon);
    QCOMPARE(changed.count(), 1);
}

void StartupViewModelTest::WithTheSimulatorOpenTheSwitchIsRefusedAndTheProcessIsNamed()
{
    Fixture fixture;
    fixture.viewModel.Show();
    fixture.processProbe.ReportTheSimulatorAsRunning();

    QCOMPARE(fixture.viewModel.Switch(kFlowExecutable, false), FileResult::TheSimulatorIsRunning);
    QCOMPARE(fixture.entries.writes, std::size_t{0});
    QCOMPARE(fixture.viewModel.RunningSimulator(), std::optional<std::string>("FlightSimulator2024.exe"));
    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{2});
}

void StartupViewModelTest::TurningOnAnEntryWhoseProgramIsGoneIsWrittenAndItsConditionStaysBroken()
{
    Fixture fixture;
    fixture.entries.Carry(StartupEntry{.label = "Ghost", .path = "C:/Program Files/Ghost/ghost.exe", .enabled = false});
    fixture.viewModel.Show();

    QCOMPARE(fixture.viewModel.Lines().back().condition, StartupCondition::Broken);
    QVERIFY(!fixture.viewModel.Lines().back().enabled);
    QCOMPARE(fixture.viewModel.Switch("C:/Program Files/Ghost/ghost.exe", true), FileResult::Completed);

    QCOMPARE(fixture.entries.writes, std::size_t{1});
    QVERIFY(fixture.viewModel.Lines().back().enabled);
    QCOMPARE(fixture.viewModel.Lines().back().condition, StartupCondition::Broken);
}

void StartupViewModelTest::AddingAnEntryWritesItAndReadsTheLinesTheSnapshotAndTheUndoPlanAgain()
{
    Fixture fixture;
    fixture.viewModel.Show();

    const QSignalSpy changed(&fixture.viewModel, &StartupViewModel::Changed);
    const StartupGestureOutcome outcome =
        fixture.viewModel.Add(StartupDraft{.label = "Any2GSX", .file = kAny2Gsx, .commandLine = "-x"});

    QCOMPARE(outcome.result, FileResult::Completed);
    QCOMPARE(outcome.entryPath, kAny2Gsx);
    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{3});
    QCOMPARE(fixture.viewModel.Lines().back().label, std::string("Any2GSX"));
    QCOMPARE(fixture.viewModel.Lines().back().commandLine, std::string("-x"));
    QCOMPARE(fixture.session.Snapshot().startupEntries.size(), std::size_t{3});
    QCOMPARE(changed.count(), 1);

    const std::optional<StartupUndoPlan> plan = fixture.viewModel.UndoPlan();

    QVERIFY(plan.has_value());
    QCOMPARE(plan->effect, StartupUndoEffect::RemovesTheAddedEntry);
    QCOMPARE(plan->label, std::string("Any2GSX"));
    QCOMPARE(fixture.journal.appended.back().kind, OperationKind::AddTheStartupEntry);
}

void StartupViewModelTest::AddingAProgramInsideAnAddonWritesThePathOfTheLink()
{
    Fixture fixture;
    fixture.viewModel.Show();

    const StartupDraftCheck check = fixture.viewModel.Check(kFlowTool, std::nullopt);

    QVERIFY(check.insideAnAddon);
    QCOMPARE(check.pathToWrite, kFlowToolByTheLink);

    QCOMPARE(fixture.viewModel.Add(StartupDraft{.label = "Tool", .file = kFlowTool}).result, FileResult::Completed);
    QCOMPARE(fixture.viewModel.Lines().back().path, kFlowToolByTheLink);
    QCOMPARE(fixture.viewModel.Lines().back().reach, StartupReach::InsideAnAddon);
}

void StartupViewModelTest::AnAddingTheEditorRefusesChangesNothingAndSaysWhy()
{
    Fixture fixture;
    fixture.viewModel.Show();

    const QSignalSpy changed(&fixture.viewModel, &StartupViewModel::Changed);
    const std::size_t writes = fixture.entries.writes;

    QCOMPARE(fixture.viewModel.Add(StartupDraft{.label = "Again", .file = kSimlink}).result,
             FileResult::TheStartupEntryIsAlreadyThere);
    QCOMPARE(fixture.viewModel.Add(StartupDraft{.label = "Nowhere", .file = "C:/Nowhere/nowhere.exe"}).result,
             FileResult::TheProgramDoesNotExist);

    QCOMPARE(fixture.entries.writes, writes);
    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{2});
    QCOMPARE(changed.count(), 0);
    QVERIFY(!fixture.viewModel.UndoPlan().has_value());
}

void StartupViewModelTest::RemovingKeepsTheEntryInTheRemovedListAndOffersTheUndo()
{
    Fixture fixture;
    fixture.viewModel.Show();

    const QSignalSpy changed(&fixture.viewModel, &StartupViewModel::Changed);

    QCOMPARE(fixture.viewModel.Remove(kSimlink).result, FileResult::Completed);

    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{1});
    QCOMPARE(fixture.session.Snapshot().startupEntries.size(), std::size_t{1});
    QCOMPARE(fixture.viewModel.Removed().size(), std::size_t{1});
    QCOMPARE(fixture.viewModel.Removed().front().entry.path, kSimlink);
    QCOMPARE(changed.count(), 1);

    const std::optional<StartupUndoPlan> plan = fixture.viewModel.UndoPlan();

    QVERIFY(plan.has_value());
    QCOMPARE(plan->effect, StartupUndoEffect::RestoresTheRemovedEntry);
    QCOMPARE(plan->label, std::string("Navigraph Simlink"));
}

void StartupViewModelTest::RestoringBringsTheEntryBackAndEmptiesTheRemovedList()
{
    Fixture fixture;
    fixture.viewModel.Show();
    QCOMPARE(fixture.viewModel.Remove(kSimlink).result, FileResult::Completed);

    QCOMPARE(fixture.viewModel.Restore(kSimlink).result, FileResult::Completed);

    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{2});
    QCOMPARE(fixture.session.Snapshot().startupEntries.size(), std::size_t{2});
    QVERIFY(fixture.viewModel.Removed().empty());
    QVERIFY2(!fixture.viewModel.UndoPlan().has_value(), "restoring what the undo would restore leaves nothing to undo");
}

void StartupViewModelTest::DiscardingForgetsTheRemovedEntryAndLeavesTheFileAlone()
{
    Fixture fixture;
    fixture.viewModel.Show();
    QCOMPARE(fixture.viewModel.Remove(kSimlink).result, FileResult::Completed);

    const std::size_t writes = fixture.entries.writes;

    QCOMPARE(fixture.viewModel.Discard(kSimlink).result, FileResult::Completed);

    QVERIFY(fixture.viewModel.Removed().empty());
    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{1});
    QCOMPARE(fixture.entries.writes, writes);
    QCOMPARE(fixture.journal.appended.back().kind, OperationKind::ForgetTheStartupEntry);
}

void StartupViewModelTest::EditingRewritesTheLineAndAPathChangeCarriesThePresets()
{
    Fixture fixture;
    fixture.viewModel.Show();

    Preset preset;
    preset.name = "Short";
    preset.governsStartup = true;
    preset.startupEntries = {PresetStartupEntry{.path = kSimlink, .action = PresetAction::Enable}};
    QVERIFY(fixture.presets.Save("msfs2024", preset));

    const StartupGestureOutcome renamed =
        fixture.viewModel.Edit(kSimlink, StartupDraft{.label = "Simlink", .file = kSimlink, .commandLine = "-q"});

    QCOMPARE(renamed.result, FileResult::Completed);
    QCOMPARE(fixture.viewModel.Lines().back().label, std::string("Simlink"));
    QCOMPARE(fixture.viewModel.Lines().back().commandLine, std::string("-q"));
    QVERIFY(renamed.presetsThatFollowed.empty());

    const StartupGestureOutcome moved =
        fixture.viewModel.Edit(kSimlink, StartupDraft{.label = "Simlink", .file = kAny2Gsx});

    QCOMPARE(moved.result, FileResult::Completed);
    QCOMPARE(moved.presetsThatFollowed, (std::vector<std::string>{"Short"}));
    QCOMPARE(fixture.viewModel.Lines().back().path, kAny2Gsx);
    QCOMPARE(fixture.session.Snapshot().startupEntries.back().path, kAny2Gsx);
    QCOMPARE(fixture.viewModel.Removed().size(), std::size_t{1});
    QCOMPARE(fixture.viewModel.Removed().front().entry.path, kSimlink);
    QCOMPARE(fixture.presets.Load("msfs2024", "Short")->startupEntries.front().path, kAny2Gsx);
    QCOMPARE(fixture.viewModel.UndoPlan()->effect, StartupUndoEffect::EditsTheEntryBack);
}

void StartupViewModelTest::APresetThatCouldNotFollowAnEditIsNamedInTheOutcome()
{
    Fixture fixture;
    fixture.viewModel.Show();

    Preset preset;
    preset.name = "Locked";
    preset.governsStartup = true;
    preset.startupEntries = {PresetStartupEntry{.path = kSimlink, .action = PresetAction::Disable}};
    QVERIFY(fixture.presets.Save("msfs2024", preset));
    fixture.presets.RefuseToSave("Locked");

    const StartupGestureOutcome outcome =
        fixture.viewModel.Edit(kSimlink, StartupDraft{.label = "Simlink", .file = kAny2Gsx});

    QCOMPARE(outcome.result, FileResult::Completed);
    QCOMPARE(outcome.presetsThatCouldNotBeWritten, (std::vector<std::string>{"Locked"}));
    QCOMPARE(fixture.viewModel.Lines().back().path, kAny2Gsx);
}

void StartupViewModelTest::UndoingGoesThroughTheEditorAndReadsAgain()
{
    Fixture fixture;
    fixture.viewModel.Show();
    QCOMPARE(fixture.viewModel.Add(StartupDraft{.label = "Any2GSX", .file = kAny2Gsx}).result, FileResult::Completed);

    const QSignalSpy changed(&fixture.viewModel, &StartupViewModel::Changed);
    const std::optional<StartupGestureOutcome> undone = fixture.viewModel.Undo();

    QVERIFY(undone.has_value());
    QCOMPARE(undone->result, FileResult::Completed);
    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{2});
    QCOMPARE(fixture.session.Snapshot().startupEntries.size(), std::size_t{2});
    QCOMPARE(fixture.viewModel.Removed().size(), std::size_t{1});
    QCOMPARE(changed.count(), 1);
    QVERIFY(!fixture.viewModel.UndoPlan().has_value());
}

void StartupViewModelTest::WithNothingToUndoThereIsNoOutcomeAndNoChange()
{
    Fixture fixture;
    fixture.viewModel.Show();

    const QSignalSpy changed(&fixture.viewModel, &StartupViewModel::Changed);

    QVERIFY(!fixture.viewModel.Undo().has_value());
    QCOMPARE(changed.count(), 0);
}

void StartupViewModelTest::TheSimulatorOpenRefusesEveryGestureAndChangesNothing()
{
    Fixture fixture;
    fixture.viewModel.Show();
    QCOMPARE(fixture.viewModel.Remove(kSimlink).result, FileResult::Completed);
    fixture.processProbe.ReportTheSimulatorAsRunning();

    const QSignalSpy changed(&fixture.viewModel, &StartupViewModel::Changed);
    const std::size_t writes = fixture.entries.writes;

    QCOMPARE(fixture.viewModel.Add(StartupDraft{.label = "Any2GSX", .file = kAny2Gsx}).result,
             FileResult::TheSimulatorIsRunning);
    QCOMPARE(fixture.viewModel.Edit(kFlowExecutable, StartupDraft{.label = "Renamed", .file = kFlowExecutable}).result,
             FileResult::TheSimulatorIsRunning);
    QCOMPARE(fixture.viewModel.Remove(kFlowExecutable).result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(fixture.viewModel.Restore(kSimlink).result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(fixture.viewModel.Undo()->result, FileResult::TheSimulatorIsRunning);

    QCOMPARE(fixture.entries.writes, writes);
    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{1});
    QCOMPARE(changed.count(), 0);
    QVERIFY2(fixture.viewModel.UndoPlan().has_value(), "a refused undo stays available for another try");
}

void StartupViewModelTest::ARefusalBecauseTheDiskDisagreesRereadsSoThePhantomLineGoes()
{
    Fixture fixture;
    fixture.viewModel.Show();
    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{2});

    StartupBackup backup;
    QCOMPARE(fixture.entries.Apply(StartupRemoval{.path = kSimlink}, backup).result, FileResult::Completed);

    QCOMPARE(fixture.viewModel.Remove(kSimlink).result, FileResult::TheDiskDisagreesWithTheScan);

    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{1});
    QCOMPARE(fixture.session.Snapshot().startupEntries.size(), std::size_t{1});
}

void StartupViewModelTest::SwitchingLeavesALineInTheJournalAndUpdatesTheSnapshotOfTheSession()
{
    Fixture fixture;
    fixture.viewModel.Show();

    QCOMPARE(fixture.viewModel.Switch(kSimlink, false), FileResult::Completed);

    QCOMPARE(fixture.journal.appended.back().kind, OperationKind::TurnOffTheStartupEntry);
    QVERIFY(!fixture.session.Snapshot().startupEntries.back().enabled);
    QVERIFY(!fixture.viewModel.UndoPlan().has_value());
}

void StartupViewModelTest::TheCheckOfAFileComesFromTheEditorAndWritesNothing()
{
    Fixture fixture;
    fixture.viewModel.Show();

    const std::size_t writes = fixture.entries.writes;
    const std::size_t lines = fixture.journal.appended.size();
    const StartupDraftCheck duplicate = fixture.viewModel.Check(kSimlink, std::nullopt);
    const StartupDraftCheck itself = fixture.viewModel.Check(kSimlink, kSimlink);
    const StartupDraftCheck moved = fixture.viewModel.Check(kAny2Gsx, kSimlink);

    QCOMPARE(duplicate.refusal, FileResult::TheStartupEntryIsAlreadyThere);
    QCOMPARE(duplicate.occupiedBy, std::string("Navigraph Simlink"));
    QCOMPARE(itself.refusal, FileResult::Completed);
    QVERIFY(!itself.changesThePath);
    QVERIFY(moved.changesThePath);
    QCOMPARE(fixture.entries.writes, writes);
    QCOMPARE(fixture.journal.appended.size(), lines);
}

void StartupViewModelTest::WithTheEntriesLeftLooseThereIsNothingToUndoAndNothingRemoved()
{
    Fixture fixture;
    fixture.viewModel.Show();
    QCOMPARE(fixture.viewModel.Remove(kSimlink).result, FileResult::Completed);
    QVERIFY(fixture.viewModel.UndoPlan().has_value());

    fixture.viewModel.Manage(false);

    QVERIFY(!fixture.viewModel.UndoPlan().has_value());
    QVERIFY(fixture.viewModel.Removed().empty());
    QVERIFY(fixture.viewModel.Lines().empty());
}

QTEST_MAIN(StartupViewModelTest)

#include "tst_startup_view_model.moc"
