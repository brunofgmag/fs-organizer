#include <QtTest/QtTest>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "application/StartupEditor.h"
#include "domain/model/Preset.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakePresetRepository.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class StartupEditorTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ACheckOutsideTheLibrariesKeepsTheChosenFileAndWritesNothing();
        static void ACheckInsideAnEnabledAddonGivesTheLinkPathOfTheAddon();
        static void ACheckInsideADisabledAddonSaysSoAndStillAcceptsIt();
        static void ACheckRefusesAPathTheFileAlreadyListsAndNamesTheEntryThatHasIt();
        static void ACheckRefusesAProgramThatDoesNotExistOnTheChosenFileAndNotOnTheLink();
        static void ACheckOfAnEditOnlyLooksForTheProgramWhenThePathChanges();
        static void ACheckOfAnEditNeverCountsTheEntryItselfAsTheOccupant();
        static void ACheckOfAnEditCountsThePresetsThatNameTheEntry();
        static void ACheckWritesTheChosenFileWithTheSeparatorOfWindows();
        static void ACheckOfAnEditThatKeepsTheFileKeepsThePathEvenInsideTheLibrary();

        static void AddingWritesTheEntryAndLeavesOneLineInTheJournal();
        static void AddingInsideAnAddonWritesTheLinkPath();
        static void AnAddingThatTheCheckRefusesWritesNothingAndLeavesItsLine();
        static void AnAddingTheSimulatorOrTheSwitchRefusesWritesNothingAndLeavesItsLine();

        static void RemovingTakesTheEntryOutKeepsItAndStampsTheMoment();
        static void RemovingAnEntryTheFileNoLongerHasIsRefusedAndLogged();
        static void RestoringBringsTheEntryBackAndLogsIt();
        static void RestoringWhatTheUndoNamesClearsTheUndoAndRestoringAnotherDoesNot();
        static void ForgettingDropsTheEntryFromTheListLogsItAndClearsTheUndoThatNamedIt();
        static void SwitchingLogsTheEntryByItsLabelWithoutAnAddonAndLeavesTheUndoAlone();
        static void ARefusedSwitchIsLoggedWithItsReason();

        static void EditingTheLabelOrTheCommandLineLeavesEveryPresetAlone();
        static void EditingThePathMakesEveryPresetFollowWithTheSameActionInOneLine();
        static void APresetThatCannotBeWrittenIsNamedAndTheEditIsKept();
        static void APresetThatAlreadyNamesTheNewPathLosesTheOldRow();
        static void AnEditTheCheckRefusesWritesNothingAndLogsTheRefusal();

        static void UndoingAnAddingRemovesTheEntry();
        static void UndoingARemovalRestoresTheEntryWhereItWas();
        static void UndoingAnEditBringsTheEntryAndThePresetsBack();
        static void UndoingAnEditThatChangedThePathForgetsTheVersionItUndidWithoutALine();
        static void UndoingAnEditThatKeptThePathLeavesTheRemovedListAlone();
        static void TheUndoIsOneLevelAndANewGestureReplacesIt();
        static void TheUndoGoesAwayWhenAGestureOfAnotherProfileArrives();
        static void AnUndoTheFileRefusesStaysAvailableForAnotherTry();
        static void AnUndoTheFileRefusesForGoodIsNoLongerOffered();
        static void AnEditThatChangesNothingKeepsTheUndoAndLeavesNoLine();
        static void RemovingOneOfTwoEntriesWithTheSamePathOffersNoUndo();
        static void WithNothingToUndoThereIsNoOutcome();

        static void TheRemovedListComesFromTheServiceNewestFirst();
    };

    const std::filesystem::path kCommunity = "E:/Flight Simulator 2024/Community";
    const std::filesystem::path kLibrary = "D:/MSFS 2024";
    const std::filesystem::path kFlowInTheLibrary = kLibrary / "Utilities" / "p42-util-flow-pro";
    const std::filesystem::path kFlowExecutableInTheLibrary = kFlowInTheLibrary / "bin" / "flow.exe";
    const std::filesystem::path kFlowLink = kCommunity / "p42-util-flow-pro";
    const std::filesystem::path kFlowExecutableByTheLink = kFlowLink / "bin" / "flow.exe";
    const std::filesystem::path kSimlink = "C:/Program Files/Navigraph/Simlink/simlink.exe";
    const std::filesystem::path kSimlinkMoved = "C:/Navigraph/Simlink/simlink.exe";
    const std::filesystem::path kAny2Gsx = "C:/Tools/Any2GSX/Any2GSX.exe";
    const std::string kProfileId = "msfs2024";
    const std::string kOtherProfileId = "msfs2020";

    [[nodiscard]] std::optional<FileResult> UndoResultOf(StartupEditor& editor)
    {
        const std::optional<StartupGestureOutcome> undone = editor.Undo(kProfileId);

        return undone.has_value() ? std::optional<FileResult>(undone->result) : std::nullopt;
    }

    SimulatorProfile Profile(const std::string& id = kProfileId)
    {
        SimulatorProfile profile;
        profile.id = id;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;

        return profile;
    }

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    ProfileSnapshot SnapshotHoldingFlow(const bool enabled)
    {
        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children.push_back(AddonNode(kFlowInTheLibrary));

        ProfileSnapshot snapshot;
        snapshot.libraries.push_back(std::move(library));
        snapshot.enabled = EnabledAddons(enabled ? std::vector<std::filesystem::path>{kFlowInTheLibrary}
                                                 : std::vector<std::filesystem::path>{});

        return snapshot;
    }

    Preset PresetNaming(const std::string& name, const std::vector<PresetStartupEntry>& entries)
    {
        Preset preset;
        preset.name = name;
        preset.governsStartup = true;
        preset.startupEntries = entries;

        return preset;
    }

    PresetStartupEntry Row(const std::filesystem::path& path, const PresetAction action)
    {
        return PresetStartupEntry{.path = path, .action = action};
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddFile(kSimlink);
            fileSystem.AddFile(kAny2Gsx);
            fileSystem.AddFile(kSimlinkMoved);
            fileSystem.AddFile(kFlowExecutableInTheLibrary);
        }

        [[nodiscard]] static StartupDraft
        Draft(const std::string& label, const std::filesystem::path& file, const std::string& commandLine = {})
        {
            return StartupDraft{.label = label, .file = file, .commandLine = commandLine};
        }

        [[nodiscard]] std::vector<std::string> LabelsInTheFile() const
        {
            std::vector<std::string> labels;

            for (const StartupEntry& entry : startup.entries.Entries())
            {
                labels.push_back(entry.label);
            }

            return labels;
        }

        [[nodiscard]] const OperationRecord& LastLine() const
        {
            static const OperationRecord nothing = OperationRecord::OfImport(
                {}, OperationKind::EnableAddon, AddonId{}, {}, {}, FileResult::ThereIsNowhereToQuarantineIt);

            return journal.appended.empty() ? nothing : journal.appended.back();
        }

        InMemoryFileSystem fileSystem;
        FakeFilesystemProbe probe{fileSystem};
        StartupOverFakes startup{probe};
        FakePresetRepository presets;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        StartupEditor editor{startup.service, presets, probe, log};
    };

    [[nodiscard]] FileResult ResultOf(const OperationRecord& record)
    {
        return std::get<FileResult>(record.outcome);
    }
}

void StartupEditorTest::ACheckOutsideTheLibrariesKeepsTheChosenFileAndWritesNothing()
{
    Fixture f;

    const StartupDraftCheck check = f.editor.Check(Profile(), SnapshotHoldingFlow(true), kSimlink, std::nullopt);

    QCOMPARE(check.pathToWrite, kSimlink);
    QVERIFY(!check.insideAnAddon);
    QVERIFY(check.addonFolderName.empty());
    QVERIFY(!check.addonIsOff);
    QCOMPARE(check.refusal, FileResult::Completed);
    QVERIFY(check.occupiedBy.empty());
    QVERIFY(!check.changesThePath);
    QCOMPARE(check.presetsNamingTheEntry, std::size_t{0});
    QCOMPARE(f.startup.entries.writes, std::size_t{0});
    QVERIFY(f.journal.appended.empty());
}

void StartupEditorTest::ACheckInsideAnEnabledAddonGivesTheLinkPathOfTheAddon()
{
    Fixture f;

    const StartupDraftCheck check =
        f.editor.Check(Profile(), SnapshotHoldingFlow(true), kFlowExecutableInTheLibrary, std::nullopt);

    QCOMPARE(check.pathToWrite, kFlowExecutableByTheLink);
    QVERIFY(check.insideAnAddon);
    QCOMPARE(check.addonFolderName, std::string("p42-util-flow-pro"));
    QVERIFY(!check.addonIsOff);
    QCOMPARE(check.refusal, FileResult::Completed);
}

void StartupEditorTest::ACheckInsideADisabledAddonSaysSoAndStillAcceptsIt()
{
    Fixture f;

    const StartupDraftCheck check =
        f.editor.Check(Profile(), SnapshotHoldingFlow(false), kFlowExecutableInTheLibrary, std::nullopt);

    QCOMPARE(check.pathToWrite, kFlowExecutableByTheLink);
    QVERIFY(check.insideAnAddon);
    QVERIFY(check.addonIsOff);
    QCOMPARE(check.refusal, FileResult::Completed);
    QVERIFY2(!f.fileSystem.Exists(kFlowExecutableByTheLink), "the link is not there while the addon is off");
}

void StartupEditorTest::ACheckRefusesAPathTheFileAlreadyListsAndNamesTheEntryThatHasIt()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink, .enabled = false});
    f.startup.entries.Carry(StartupEntry{.label = "Flow by its link", .path = kFlowExecutableByTheLink});

    const StartupDraftCheck sameTextWithAnotherCase = f.editor.Check(
        Profile(), SnapshotHoldingFlow(true), "c:\\program files\\NAVIGRAPH\\simlink\\SIMLINK.EXE", std::nullopt);

    QCOMPARE(sameTextWithAnotherCase.refusal, FileResult::TheStartupEntryIsAlreadyThere);
    QCOMPARE(sameTextWithAnotherCase.occupiedBy, std::string("Navigraph Simlink"));

    const StartupDraftCheck byTheLink =
        f.editor.Check(Profile(), SnapshotHoldingFlow(true), kFlowExecutableInTheLibrary, std::nullopt);

    QCOMPARE(byTheLink.refusal, FileResult::TheStartupEntryIsAlreadyThere);
    QCOMPARE(byTheLink.occupiedBy, std::string("Flow by its link"));
}

void StartupEditorTest::ACheckRefusesAProgramThatDoesNotExistOnTheChosenFileAndNotOnTheLink()
{
    Fixture f;

    const StartupDraftCheck missing =
        f.editor.Check(Profile(), SnapshotHoldingFlow(true), "C:/Tools/Gone/gone.exe", std::nullopt);

    QCOMPARE(missing.refusal, FileResult::TheProgramDoesNotExist);
    QVERIFY(missing.occupiedBy.empty());

    const StartupDraftCheck throughTheLink =
        f.editor.Check(Profile(), SnapshotHoldingFlow(false), kFlowExecutableInTheLibrary, std::nullopt);

    QCOMPARE(throughTheLink.refusal, FileResult::Completed);
}

void StartupEditorTest::ACheckOfAnEditOnlyLooksForTheProgramWhenThePathChanges()
{
    Fixture f;
    const std::filesystem::path gone = "C:/Tools/Gone/gone.exe";
    f.startup.entries.Carry(StartupEntry{.label = "Gone", .path = gone});

    const StartupDraftCheck renaming = f.editor.Check(Profile(), SnapshotHoldingFlow(true), gone, gone);

    QCOMPARE(renaming.refusal, FileResult::Completed);
    QVERIFY(!renaming.changesThePath);

    const StartupDraftCheck movingToAMissingOne =
        f.editor.Check(Profile(), SnapshotHoldingFlow(true), "C:/Tools/Also/gone.exe", gone);

    QCOMPARE(movingToAMissingOne.refusal, FileResult::TheProgramDoesNotExist);
    QVERIFY(movingToAMissingOne.changesThePath);

    const StartupDraftCheck movingToARealOne = f.editor.Check(Profile(), SnapshotHoldingFlow(true), kSimlink, gone);

    QCOMPARE(movingToARealOne.refusal, FileResult::Completed);
    QCOMPARE(movingToARealOne.pathToWrite, kSimlink);
    QVERIFY(movingToARealOne.changesThePath);
}

void StartupEditorTest::ACheckOfAnEditNeverCountsTheEntryItselfAsTheOccupant()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    f.startup.entries.Carry(StartupEntry{.label = "Any2GSX", .path = kAny2Gsx});

    const StartupDraftCheck itself = f.editor.Check(Profile(), SnapshotHoldingFlow(true), kSimlink, kSimlink);

    QCOMPARE(itself.refusal, FileResult::Completed);
    QVERIFY(itself.occupiedBy.empty());

    const StartupDraftCheck another = f.editor.Check(Profile(), SnapshotHoldingFlow(true), kAny2Gsx, kSimlink);

    QCOMPARE(another.refusal, FileResult::TheStartupEntryIsAlreadyThere);
    QCOMPARE(another.occupiedBy, std::string("Any2GSX"));
}

void StartupEditorTest::ACheckOfAnEditCountsThePresetsThatNameTheEntry()
{
    Fixture f;
    QVERIFY(f.presets.Save(kProfileId, PresetNaming("Short", {Row(kSimlink, PresetAction::Enable)})));
    QVERIFY(f.presets.Save(
        kProfileId,
        PresetNaming("Long",
                     {Row(kAny2Gsx, PresetAction::Enable),
                      Row("C:\\PROGRAM FILES\\navigraph\\Simlink\\simlink.exe", PresetAction::Disable)})));
    QVERIFY(f.presets.Save(kProfileId, PresetNaming("Other", {Row(kAny2Gsx, PresetAction::Enable)})));
    QVERIFY(f.presets.Save(kOtherProfileId, PresetNaming("Short", {Row(kSimlink, PresetAction::Enable)})));

    const StartupDraftCheck check = f.editor.Check(Profile(), SnapshotHoldingFlow(true), kSimlinkMoved, kSimlink);

    QCOMPARE(check.presetsNamingTheEntry, std::size_t{2});
    QCOMPARE(f.editor.Check(Profile(), SnapshotHoldingFlow(true), kSimlinkMoved, std::nullopt).presetsNamingTheEntry,
             std::size_t{0});
}

void StartupEditorTest::ACheckWritesTheChosenFileWithTheSeparatorOfWindows()
{
    Fixture f;

    const StartupDraftCheck check = f.editor.Check(Profile(), SnapshotHoldingFlow(true), kSimlink, std::nullopt);

    QCOMPARE(AsUtf8(check.pathToWrite), std::string(R"(C:\Program Files\Navigraph\Simlink\simlink.exe)"));
    QCOMPARE(check.refusal, FileResult::Completed);

    const StartupDraftCheck inside =
        f.editor.Check(Profile(), SnapshotHoldingFlow(true), kFlowExecutableInTheLibrary, std::nullopt);

    QVERIFY(AsUtf8(inside.pathToWrite).find('/') == std::string::npos);
}

void StartupEditorTest::ACheckOfAnEditThatKeepsTheFileKeepsThePathEvenInsideTheLibrary()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Flow", .path = kFlowExecutableInTheLibrary});

    const StartupDraftCheck check =
        f.editor.Check(Profile(), SnapshotHoldingFlow(true), kFlowExecutableInTheLibrary, kFlowExecutableInTheLibrary);

    QCOMPARE(check.pathToWrite, kFlowExecutableInTheLibrary);
    QVERIFY(!check.insideAnAddon);
    QVERIFY(!check.changesThePath);
    QCOMPARE(check.refusal, FileResult::Completed);

    const StartupGestureOutcome renamed =
        f.editor.Edit(Profile(), SnapshotHoldingFlow(true), kFlowExecutableInTheLibrary,
                      Fixture::Draft("Flow, renamed", kFlowExecutableInTheLibrary));

    QCOMPARE(renamed.result, FileResult::Completed);
    QCOMPARE(f.startup.entries.Entries().at(0).path, kFlowExecutableInTheLibrary);
    QCOMPARE(f.startup.entries.Entries().at(0).label, std::string("Flow, renamed"));

    const StartupDraftCheck another = f.editor.Check(
        Profile(), SnapshotHoldingFlow(true), kFlowInTheLibrary / "bin" / "other.exe", kFlowExecutableInTheLibrary);

    QVERIFY(another.insideAnAddon);
    QVERIFY(another.changesThePath);
}

void StartupEditorTest::AddingWritesTheEntryAndLeavesOneLineInTheJournal()
{
    Fixture f;

    const StartupGestureOutcome outcome =
        f.editor.Add(Profile(), SnapshotHoldingFlow(true), Fixture::Draft("Navigraph Simlink", kSimlink, "-x"));

    QCOMPARE(outcome.result, FileResult::Completed);
    QCOMPARE(outcome.entryPath, kSimlink);

    const std::vector<StartupEntry> entries = f.startup.entries.Entries();

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().label, std::string("Navigraph Simlink"));
    QCOMPARE(entries.front().commandLine, std::string("-x"));
    QVERIFY(entries.front().enabled);

    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(f.LastLine().kind, OperationKind::AddTheStartupEntry);
    QCOMPARE(f.LastLine().label, std::string("Navigraph Simlink"));
    QCOMPARE(f.LastLine().target, kSimlink);
    QVERIFY(f.LastLine().addonId.folderName.empty());
    QCOMPARE(ResultOf(f.LastLine()), FileResult::Completed);
    QCOMPARE(f.LastLine().timestamp, f.clock.now);
}

void StartupEditorTest::AddingInsideAnAddonWritesTheLinkPath()
{
    Fixture f;

    QCOMPARE(
        f.editor.Add(Profile(), SnapshotHoldingFlow(false), Fixture::Draft("Flow", kFlowExecutableInTheLibrary)).result,
        FileResult::Completed);

    QCOMPARE(f.startup.entries.Entries().at(0).path, kFlowExecutableByTheLink);
    QCOMPARE(f.LastLine().target, kFlowExecutableByTheLink);
}

void StartupEditorTest::AnAddingThatTheCheckRefusesWritesNothingAndLeavesItsLine()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    const std::size_t writes = f.startup.entries.writes;

    QCOMPARE(f.editor.Add(Profile(), SnapshotHoldingFlow(true), Fixture::Draft("Again", kSimlink)).result,
             FileResult::TheStartupEntryIsAlreadyThere);
    QCOMPARE(f.LastLine().kind, OperationKind::AddTheStartupEntry);
    QCOMPARE(f.LastLine().label, std::string("Again"));
    QCOMPARE(ResultOf(f.LastLine()), FileResult::TheStartupEntryIsAlreadyThere);

    QCOMPARE(f.editor.Add(Profile(), SnapshotHoldingFlow(true), Fixture::Draft("Gone", "C:/Tools/gone.exe")).result,
             FileResult::TheProgramDoesNotExist);
    QCOMPARE(ResultOf(f.LastLine()), FileResult::TheProgramDoesNotExist);

    QCOMPARE(f.journal.appended.size(), std::size_t{2});
    QCOMPARE(f.startup.entries.writes, writes);
    QCOMPARE(f.startup.entries.Entries().size(), std::size_t{1});
    QVERIFY(!f.editor.WhatUndoWouldDo(kProfileId).has_value());
}

void StartupEditorTest::AnAddingTheSimulatorOrTheSwitchRefusesWritesNothingAndLeavesItsLine()
{
    Fixture f;
    f.startup.processProbe.ReportTheSimulatorAsRunning();

    QCOMPARE(f.editor.Add(Profile(), SnapshotHoldingFlow(true), Fixture::Draft("Simlink", kSimlink)).result,
             FileResult::TheSimulatorIsRunning);
    QCOMPARE(ResultOf(f.LastLine()), FileResult::TheSimulatorIsRunning);

    f.startup.processProbe.ReportTheSimulatorAsClosed();
    f.startup.service.Manage(false);

    QCOMPARE(f.editor.Add(Profile(), SnapshotHoldingFlow(true), Fixture::Draft("Simlink", kSimlink)).result,
             FileResult::TheStartupEntriesAreLeftLoose);
    QCOMPARE(ResultOf(f.LastLine()), FileResult::TheStartupEntriesAreLeftLoose);

    QCOMPARE(f.journal.appended.size(), std::size_t{2});
    QCOMPARE(f.startup.entries.writes, std::size_t{0});
    QVERIFY(!f.editor.WhatUndoWouldDo(kProfileId).has_value());
}

void StartupEditorTest::RemovingTakesTheEntryOutKeepsItAndStampsTheMoment()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink, .commandLine = "-x"});
    f.startup.entries.Carry(StartupEntry{.label = "Any2GSX", .path = kAny2Gsx});

    const StartupGestureOutcome outcome = f.editor.Remove(kProfileId, kSimlink);

    QCOMPARE(outcome.result, FileResult::Completed);
    QCOMPARE(f.LabelsInTheFile(), (std::vector<std::string>{"Any2GSX"}));

    const std::vector<StartupRemovedEntry> removed = f.editor.Removed();

    QCOMPARE(removed.size(), std::size_t{1});
    QCOMPARE(removed.front().entry.label, std::string("Navigraph Simlink"));
    QCOMPARE(removed.front().removedAt, f.clock.now);

    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(f.LastLine().kind, OperationKind::RemoveTheStartupEntry);
    QCOMPARE(f.LastLine().label, std::string("Navigraph Simlink"));
    QCOMPARE(f.LastLine().target, kSimlink);
    QCOMPARE(ResultOf(f.LastLine()), FileResult::Completed);

    const std::optional<StartupUndoPlan> plan = f.editor.WhatUndoWouldDo(kProfileId);

    QVERIFY(plan.has_value());
    QCOMPARE(plan->effect, StartupUndoEffect::RestoresTheRemovedEntry);
    QCOMPARE(plan->label, std::string("Navigraph Simlink"));
}

void StartupEditorTest::RemovingAnEntryTheFileNoLongerHasIsRefusedAndLogged()
{
    Fixture f;

    const StartupGestureOutcome outcome = f.editor.Remove(kProfileId, kSimlink);

    QCOMPARE(outcome.result, FileResult::TheDiskDisagreesWithTheScan);
    QCOMPARE(f.startup.entries.writes, std::size_t{0});
    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(ResultOf(f.LastLine()), FileResult::TheDiskDisagreesWithTheScan);
    QCOMPARE(f.LastLine().target, kSimlink);
    QVERIFY(!f.editor.WhatUndoWouldDo(kProfileId).has_value());
}

void StartupEditorTest::RestoringBringsTheEntryBackAndLogsIt()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    QCOMPARE(f.editor.Remove(kProfileId, kSimlink).result, FileResult::Completed);

    QCOMPARE(f.editor.Restore(kProfileId, kSimlink).result, FileResult::Completed);

    QCOMPARE(f.LabelsInTheFile(), (std::vector<std::string>{"Navigraph Simlink"}));
    QVERIFY(f.editor.Removed().empty());
    QCOMPARE(f.LastLine().kind, OperationKind::RestoreTheStartupEntry);
    QCOMPARE(f.LastLine().label, std::string("Navigraph Simlink"));
    QCOMPARE(f.LastLine().target, kSimlink);

    QCOMPARE(f.editor.Restore(kProfileId, kSimlink).result, FileResult::TheDiskDisagreesWithTheScan);
    QCOMPARE(ResultOf(f.LastLine()), FileResult::TheDiskDisagreesWithTheScan);
}

void StartupEditorTest::RestoringWhatTheUndoNamesClearsTheUndoAndRestoringAnotherDoesNot()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    f.startup.entries.Carry(StartupEntry{.label = "Any2GSX", .path = kAny2Gsx});
    f.startup.entries.CarryRemoved(StartupEntry{.label = "Old", .path = kSimlinkMoved});
    QCOMPARE(f.editor.Remove(kProfileId, kSimlink).result, FileResult::Completed);

    QCOMPARE(f.editor.Restore(kProfileId, kSimlinkMoved).result, FileResult::Completed);
    QVERIFY(f.editor.WhatUndoWouldDo(kProfileId).has_value());

    QCOMPARE(f.editor.Restore(kProfileId, "C:\\PROGRAM FILES\\navigraph\\simlink\\SIMLINK.EXE").result,
             FileResult::Completed);
    QVERIFY(!f.editor.WhatUndoWouldDo(kProfileId).has_value());
}

void StartupEditorTest::ForgettingDropsTheEntryFromTheListLogsItAndClearsTheUndoThatNamedIt()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    f.startup.entries.CarryRemoved(StartupEntry{.label = "Old", .path = kSimlinkMoved});
    QCOMPARE(f.editor.Remove(kProfileId, kSimlink).result, FileResult::Completed);

    QCOMPARE(f.editor.Forget(kProfileId, kSimlinkMoved).result, FileResult::Completed);

    QCOMPARE(f.LastLine().kind, OperationKind::ForgetTheStartupEntry);
    QCOMPARE(f.LastLine().label, std::string("Old"));
    QCOMPARE(f.LastLine().target, kSimlinkMoved);
    QCOMPARE(f.editor.Removed().size(), std::size_t{1});
    QVERIFY2(f.editor.WhatUndoWouldDo(kProfileId).has_value(), "forgetting another entry leaves the undo alone");

    QCOMPARE(f.editor.Forget(kProfileId, kSimlink).result, FileResult::Completed);
    QVERIFY(f.editor.Removed().empty());
    QVERIFY(!f.editor.WhatUndoWouldDo(kProfileId).has_value());
}

void StartupEditorTest::SwitchingLogsTheEntryByItsLabelWithoutAnAddonAndLeavesTheUndoAlone()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    f.startup.entries.Carry(StartupEntry{.label = "Any2GSX", .path = kAny2Gsx});
    QCOMPARE(f.editor.Remove(kProfileId, kAny2Gsx).result, FileResult::Completed);

    QCOMPARE(f.editor.Switch(kSimlink, false).result, FileResult::Completed);

    QVERIFY(!f.startup.entries.Entries().at(0).enabled);
    QCOMPARE(f.LastLine().kind, OperationKind::TurnOffTheStartupEntry);
    QCOMPARE(f.LastLine().label, std::string("Navigraph Simlink"));
    QCOMPARE(f.LastLine().target, kSimlink);
    QVERIFY(f.LastLine().addonId.folderName.empty());

    QCOMPARE(f.editor.Switch(kSimlink, true).result, FileResult::Completed);
    QCOMPARE(f.LastLine().kind, OperationKind::TurnOnTheStartupEntry);

    const std::optional<StartupUndoPlan> plan = f.editor.WhatUndoWouldDo(kProfileId);

    QVERIFY(plan.has_value());
    QCOMPARE(plan->label, std::string("Any2GSX"));
}

void StartupEditorTest::ARefusedSwitchIsLoggedWithItsReason()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    f.startup.processProbe.ReportTheSimulatorAsRunning();

    QCOMPARE(f.editor.Switch(kSimlink, false).result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(ResultOf(f.LastLine()), FileResult::TheSimulatorIsRunning);
    QCOMPARE(f.LastLine().label, std::string("Navigraph Simlink"));
    QCOMPARE(f.startup.entries.writes, std::size_t{0});
}

void StartupEditorTest::EditingTheLabelOrTheCommandLineLeavesEveryPresetAlone()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    QVERIFY(f.presets.Save(kProfileId, PresetNaming("Short", {Row(kSimlink, PresetAction::Enable)})));
    f.presets.RefuseEveryWrite();

    const StartupGestureOutcome outcome =
        f.editor.Edit(Profile(), SnapshotHoldingFlow(true), kSimlink, Fixture::Draft("Simlink 2", kSimlink, "--fast"));

    QCOMPARE(outcome.result, FileResult::Completed);
    QVERIFY(outcome.presetsThatFollowed.empty());
    QVERIFY(outcome.presetsThatCouldNotBeWritten.empty());
    QVERIFY(!outcome.returnPresetFollowed);
    QVERIFY(!outcome.returnPresetCouldNotBeWritten);
    QCOMPARE(f.startup.entries.Entries().at(0).label, std::string("Simlink 2"));
    QCOMPARE(f.startup.entries.Entries().at(0).commandLine, std::string("--fast"));

    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(f.LastLine().kind, OperationKind::EditTheStartupEntry);
    QCOMPARE(f.LastLine().label, std::string("Simlink 2"));
    QCOMPARE(f.LastLine().target, kSimlink);
    QVERIFY(f.LastLine().source.empty());
}

void StartupEditorTest::EditingThePathMakesEveryPresetFollowWithTheSameActionInOneLine()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    QVERIFY(f.presets.Save(kProfileId, PresetNaming("Short", {Row(kSimlink, PresetAction::Enable)})));
    QVERIFY(f.presets.Save(
        kProfileId,
        PresetNaming("Long",
                     {Row(kAny2Gsx, PresetAction::Enable),
                      Row("c:\\program files\\navigraph\\simlink\\simlink.exe", PresetAction::Disable)})));
    QVERIFY(f.presets.Save(kProfileId, PresetNaming("Without", {Row(kAny2Gsx, PresetAction::Enable)})));
    QVERIFY(f.presets.Save(kOtherProfileId, PresetNaming("Short", {Row(kSimlink, PresetAction::Enable)})));
    QVERIFY(f.presets.SaveReturnPreset(kProfileId, PresetNaming("return", {Row(kSimlink, PresetAction::Enable)})));

    const StartupGestureOutcome outcome = f.editor.Edit(Profile(), SnapshotHoldingFlow(true), kSimlink,
                                                        Fixture::Draft("Navigraph Simlink", kSimlinkMoved));

    QCOMPARE(outcome.result, FileResult::Completed);
    QCOMPARE(outcome.entryPath, kSimlinkMoved);
    QCOMPARE(outcome.presetsThatFollowed, (std::vector<std::string>{"Long", "Short"}));
    QVERIFY(outcome.presetsThatCouldNotBeWritten.empty());
    QVERIFY(outcome.returnPresetFollowed);

    const Preset shortOne = *f.presets.Load(kProfileId, "Short");
    const Preset longOne = *f.presets.Load(kProfileId, "Long");

    QCOMPARE(shortOne.startupEntries.front().path, kSimlinkMoved);
    QVERIFY(shortOne.startupEntries.front().action == PresetAction::Enable);
    QCOMPARE(longOne.startupEntries.front().path, kAny2Gsx);
    QCOMPARE(longOne.startupEntries.back().path, kSimlinkMoved);
    QVERIFY(longOne.startupEntries.back().action == PresetAction::Disable);
    QCOMPARE(f.presets.Load(kProfileId, "Without")->startupEntries.front().path, kAny2Gsx);
    QCOMPARE(f.presets.Load(kOtherProfileId, "Short")->startupEntries.front().path, kSimlink);
    QCOMPARE(f.presets.LoadReturnPreset(kProfileId)->startupEntries.front().path, kSimlinkMoved);

    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(f.LastLine().kind, OperationKind::EditTheStartupEntry);
    QCOMPARE(f.LastLine().source, kSimlink);
    QCOMPARE(f.LastLine().target, kSimlinkMoved);
    QCOMPARE(f.LastLine().label, std::string("Navigraph Simlink"));
}

void StartupEditorTest::APresetThatCannotBeWrittenIsNamedAndTheEditIsKept()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    QVERIFY(f.presets.Save(kProfileId, PresetNaming("Short", {Row(kSimlink, PresetAction::Enable)})));
    QVERIFY(f.presets.Save(kProfileId, PresetNaming("Locked", {Row(kSimlink, PresetAction::Enable)})));
    f.presets.RefuseToSave("Locked");

    const StartupGestureOutcome outcome = f.editor.Edit(Profile(), SnapshotHoldingFlow(true), kSimlink,
                                                        Fixture::Draft("Navigraph Simlink", kSimlinkMoved));

    QCOMPARE(outcome.result, FileResult::Completed);
    QCOMPARE(outcome.presetsThatFollowed, (std::vector<std::string>{"Short"}));
    QCOMPARE(outcome.presetsThatCouldNotBeWritten, (std::vector<std::string>{"Locked"}));
    QCOMPARE(f.startup.entries.Entries().at(0).path, kSimlinkMoved);
    QCOMPARE(f.presets.Load(kProfileId, "Locked")->startupEntries.front().path, kSimlink);
    QCOMPARE(ResultOf(f.LastLine()), FileResult::Completed);
}

void StartupEditorTest::APresetThatAlreadyNamesTheNewPathLosesTheOldRow()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    QVERIFY(f.presets.Save(
        kProfileId,
        PresetNaming("Both", {Row(kSimlink, PresetAction::Enable), Row(kSimlinkMoved, PresetAction::Disable)})));

    QCOMPARE(
        f.editor
            .Edit(Profile(), SnapshotHoldingFlow(true), kSimlink, Fixture::Draft("Navigraph Simlink", kSimlinkMoved))
            .presetsThatFollowed,
        (std::vector<std::string>{"Both"}));

    const Preset both = *f.presets.Load(kProfileId, "Both");

    QCOMPARE(both.startupEntries.size(), std::size_t{1});
    QCOMPARE(both.startupEntries.front().path, kSimlinkMoved);
    QVERIFY(both.startupEntries.front().action == PresetAction::Disable);
}

void StartupEditorTest::AnEditTheCheckRefusesWritesNothingAndLogsTheRefusal()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    f.startup.entries.Carry(StartupEntry{.label = "Any2GSX", .path = kAny2Gsx});
    QVERIFY(f.presets.Save(kProfileId, PresetNaming("Short", {Row(kSimlink, PresetAction::Enable)})));
    const std::size_t writes = f.startup.entries.writes;

    const StartupGestureOutcome outcome =
        f.editor.Edit(Profile(), SnapshotHoldingFlow(true), kSimlink, Fixture::Draft("Simlink", kAny2Gsx));

    QCOMPARE(outcome.result, FileResult::TheStartupEntryIsAlreadyThere);
    QCOMPARE(f.startup.entries.writes, writes);
    QCOMPARE(f.presets.Load(kProfileId, "Short")->startupEntries.front().path, kSimlink);
    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(ResultOf(f.LastLine()), FileResult::TheStartupEntryIsAlreadyThere);
    QVERIFY(!f.editor.WhatUndoWouldDo(kProfileId).has_value());
}

void StartupEditorTest::UndoingAnAddingRemovesTheEntry()
{
    Fixture f;
    QCOMPARE(f.editor.Add(Profile(), SnapshotHoldingFlow(true), Fixture::Draft("Navigraph Simlink", kSimlink)).result,
             FileResult::Completed);

    const std::optional<StartupUndoPlan> plan = f.editor.WhatUndoWouldDo(kProfileId);

    QVERIFY(plan.has_value());
    QCOMPARE(plan->effect, StartupUndoEffect::RemovesTheAddedEntry);
    QCOMPARE(plan->label, std::string("Navigraph Simlink"));

    const std::optional<StartupGestureOutcome> undone = f.editor.Undo(kProfileId);

    QVERIFY(undone.has_value());
    QCOMPARE(undone->result, FileResult::Completed);
    QVERIFY(f.startup.entries.Entries().empty());
    QCOMPARE(f.LastLine().kind, OperationKind::RemoveTheStartupEntry);
    QCOMPARE(f.editor.Removed().size(), std::size_t{1});
    QVERIFY(!f.editor.WhatUndoWouldDo(kProfileId).has_value());
    QVERIFY(!f.editor.Undo(kProfileId).has_value());
}

void StartupEditorTest::UndoingARemovalRestoresTheEntryWhereItWas()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "First", .path = kSimlink});
    f.startup.entries.Carry(StartupEntry{.label = "Second", .path = kAny2Gsx, .commandLine = "-s", .enabled = false});
    f.startup.entries.Carry(StartupEntry{.label = "Third", .path = kSimlinkMoved});
    QCOMPARE(f.editor.Remove(kProfileId, kAny2Gsx).result, FileResult::Completed);

    const std::optional<StartupUndoPlan> plan = f.editor.WhatUndoWouldDo(kProfileId);

    QVERIFY(plan.has_value());
    QCOMPARE(plan->effect, StartupUndoEffect::RestoresTheRemovedEntry);

    const std::optional<StartupGestureOutcome> undone = f.editor.Undo(kProfileId);

    QVERIFY(undone.has_value());
    QCOMPARE(undone->result, FileResult::Completed);
    QCOMPARE(f.LabelsInTheFile(), (std::vector<std::string>{"First", "Second", "Third"}));
    QVERIFY(!f.startup.entries.Entries().at(1).enabled);
    QCOMPARE(f.LastLine().kind, OperationKind::RestoreTheStartupEntry);
    QVERIFY(!f.editor.WhatUndoWouldDo(kProfileId).has_value());
}

void StartupEditorTest::UndoingAnEditBringsTheEntryAndThePresetsBack()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink, .commandLine = "-x"});
    QVERIFY(f.presets.Save(kProfileId, PresetNaming("Short", {Row(kSimlink, PresetAction::Disable)})));
    QCOMPARE(f.editor
                 .Edit(Profile(), SnapshotHoldingFlow(true), kSimlink,
                       Fixture::Draft("Simlink, moved", kSimlinkMoved, "--other"))
                 .result,
             FileResult::Completed);
    QCOMPARE(f.presets.Load(kProfileId, "Short")->startupEntries.front().path, kSimlinkMoved);

    const std::optional<StartupUndoPlan> plan = f.editor.WhatUndoWouldDo(kProfileId);

    QVERIFY(plan.has_value());
    QCOMPARE(plan->effect, StartupUndoEffect::EditsTheEntryBack);
    QCOMPARE(plan->label, std::string("Simlink, moved"));

    const std::optional<StartupGestureOutcome> undone = f.editor.Undo(kProfileId);

    QVERIFY(undone.has_value());
    QCOMPARE(undone->result, FileResult::Completed);
    QCOMPARE(undone->presetsThatFollowed, (std::vector<std::string>{"Short"}));

    const std::vector<StartupEntry> entries = f.startup.entries.Entries();

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().label, std::string("Navigraph Simlink"));
    QCOMPARE(entries.front().path, kSimlink);
    QCOMPARE(entries.front().commandLine, std::string("-x"));
    QCOMPARE(f.presets.Load(kProfileId, "Short")->startupEntries.front().path, kSimlink);
    QVERIFY(f.presets.Load(kProfileId, "Short")->startupEntries.front().action == PresetAction::Disable);
    QCOMPARE(f.LastLine().kind, OperationKind::EditTheStartupEntry);
    QCOMPARE(f.LastLine().source, kSimlinkMoved);
    QCOMPARE(f.LastLine().target, kSimlink);
    QVERIFY(!f.editor.WhatUndoWouldDo(kProfileId).has_value());
}

void StartupEditorTest::UndoingAnEditThatChangedThePathForgetsTheVersionItUndidWithoutALine()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    f.startup.entries.Carry(StartupEntry{.label = "Any2GSX", .path = kAny2Gsx});
    QCOMPARE(f.editor.Remove(kProfileId, kAny2Gsx).result, FileResult::Completed);
    QCOMPARE(
        f.editor.Edit(Profile(), SnapshotHoldingFlow(true), kSimlink, Fixture::Draft("Simlink, moved", kSimlinkMoved))
            .result,
        FileResult::Completed);

    QCOMPARE(f.editor.Removed().size(), std::size_t{2});

    const std::size_t linesBefore = f.journal.appended.size();

    QCOMPARE(UndoResultOf(f.editor), std::optional<FileResult>(FileResult::Completed));

    const std::vector<StartupRemovedEntry> removed = f.editor.Removed();

    QCOMPARE(removed.size(), std::size_t{1});
    QCOMPARE(removed.front().entry.path, kAny2Gsx);
    QCOMPARE(f.journal.appended.size(), linesBefore + 1);
    QCOMPARE(f.LastLine().kind, OperationKind::EditTheStartupEntry);
}

void StartupEditorTest::UndoingAnEditThatKeptThePathLeavesTheRemovedListAlone()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    f.startup.entries.Carry(StartupEntry{.label = "Any2GSX", .path = kAny2Gsx});
    QCOMPARE(f.editor.Remove(kProfileId, kAny2Gsx).result, FileResult::Completed);
    QCOMPARE(f.editor.Edit(Profile(), SnapshotHoldingFlow(true), kSimlink, Fixture::Draft("Renamed", kSimlink)).result,
             FileResult::Completed);

    QCOMPARE(UndoResultOf(f.editor), std::optional<FileResult>(FileResult::Completed));

    QCOMPARE(f.editor.Removed().size(), std::size_t{1});
    QCOMPARE(f.editor.Removed().front().entry.path, kAny2Gsx);
    QCOMPARE(f.LabelsInTheFile(), (std::vector<std::string>{"Navigraph Simlink"}));
}

void StartupEditorTest::TheUndoIsOneLevelAndANewGestureReplacesIt()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "First", .path = kSimlink});
    f.startup.entries.Carry(StartupEntry{.label = "Second", .path = kAny2Gsx});

    QCOMPARE(f.editor.Remove(kProfileId, kSimlink).result, FileResult::Completed);
    QCOMPARE(f.editor.Remove(kProfileId, kAny2Gsx).result, FileResult::Completed);

    const std::optional<StartupUndoPlan> plan = f.editor.WhatUndoWouldDo(kProfileId);

    QVERIFY(plan.has_value());
    QCOMPARE(plan->label, std::string("Second"));
    QVERIFY(f.editor.Undo(kProfileId).has_value());
    QCOMPARE(f.LabelsInTheFile(), (std::vector<std::string>{"Second"}));
    QVERIFY(!f.editor.Undo(kProfileId).has_value());
}

void StartupEditorTest::TheUndoGoesAwayWhenAGestureOfAnotherProfileArrives()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "First", .path = kSimlink});
    QCOMPARE(f.editor.Remove(kProfileId, kSimlink).result, FileResult::Completed);

    QVERIFY(f.editor.WhatUndoWouldDo(kProfileId).has_value());
    QVERIFY2(!f.editor.WhatUndoWouldDo(kOtherProfileId).has_value(), "another profile never sees it");
    QVERIFY(!f.editor.Undo(kOtherProfileId).has_value());
    QVERIFY2(f.editor.WhatUndoWouldDo(kProfileId).has_value(), "looking from another profile does not clear it");

    QCOMPARE(f.editor.Remove(kOtherProfileId, "C:/Nothing.exe").result, FileResult::TheDiskDisagreesWithTheScan);

    QVERIFY(!f.editor.WhatUndoWouldDo(kProfileId).has_value());
    QVERIFY(!f.editor.Undo(kProfileId).has_value());
}

void StartupEditorTest::AnUndoTheFileRefusesStaysAvailableForAnotherTry()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "First", .path = kSimlink});
    QCOMPARE(f.editor.Remove(kProfileId, kSimlink).result, FileResult::Completed);
    f.startup.processProbe.ReportTheSimulatorAsRunning();

    const std::optional<StartupGestureOutcome> refused = f.editor.Undo(kProfileId);

    QVERIFY(refused.has_value());
    QCOMPARE(refused->result, FileResult::TheSimulatorIsRunning);
    QVERIFY(f.startup.entries.Entries().empty());
    QVERIFY(f.editor.WhatUndoWouldDo(kProfileId).has_value());

    f.startup.processProbe.ReportTheSimulatorAsClosed();

    QCOMPARE(UndoResultOf(f.editor), std::optional<FileResult>(FileResult::Completed));
    QCOMPARE(f.LabelsInTheFile(), (std::vector<std::string>{"First"}));
}

void StartupEditorTest::AnUndoTheFileRefusesForGoodIsNoLongerOffered()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "First", .path = kSimlink});
    QCOMPARE(f.editor.Remove(kProfileId, kSimlink).result, FileResult::Completed);
    f.startup.entries.Carry(StartupEntry{.label = "Back by hand", .path = kSimlink});

    const std::optional<StartupGestureOutcome> alreadyThere = f.editor.Undo(kProfileId);

    QVERIFY(alreadyThere.has_value());
    QCOMPARE(alreadyThere->result, FileResult::TheStartupEntryIsAlreadyThere);
    QVERIFY2(!f.editor.WhatUndoWouldDo(kProfileId).has_value(), "the file will not change its mind");

    QCOMPARE(f.editor.Add(Profile(), SnapshotHoldingFlow(true), Fixture::Draft("Any2GSX", kAny2Gsx)).result,
             FileResult::Completed);

    StartupBackup backup;
    QCOMPARE(f.startup.entries.Apply(StartupRemoval{.path = kAny2Gsx}, backup).result, FileResult::Completed);

    const std::optional<StartupGestureOutcome> disagrees = f.editor.Undo(kProfileId);

    QVERIFY(disagrees.has_value());
    QCOMPARE(disagrees->result, FileResult::TheDiskDisagreesWithTheScan);
    QVERIFY(!f.editor.WhatUndoWouldDo(kProfileId).has_value());
}

void StartupEditorTest::AnEditThatChangesNothingKeepsTheUndoAndLeavesNoLine()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink});
    f.startup.entries.Carry(StartupEntry{.label = "Any2GSX", .path = kAny2Gsx});
    QCOMPARE(f.editor.Remove(kProfileId, kAny2Gsx).result, FileResult::Completed);

    const std::size_t lines = f.journal.appended.size();

    const StartupGestureOutcome same =
        f.editor.Edit(Profile(), SnapshotHoldingFlow(true), kSimlink, Fixture::Draft("Navigraph Simlink", kSimlink));

    QCOMPARE(same.result, FileResult::Completed);
    QVERIFY(same.changedNothing);
    QCOMPARE(f.journal.appended.size(), lines);
    QCOMPARE(f.editor.WhatUndoWouldDo(kProfileId)->effect, StartupUndoEffect::RestoresTheRemovedEntry);

    const StartupGestureOutcome renamed =
        f.editor.Edit(Profile(), SnapshotHoldingFlow(true), kSimlink, Fixture::Draft("Simlink", kSimlink));

    QVERIFY(!renamed.changedNothing);
    QCOMPARE(f.journal.appended.size(), lines + 1);
    QCOMPARE(f.editor.WhatUndoWouldDo(kProfileId)->effect, StartupUndoEffect::EditsTheEntryBack);
}

void StartupEditorTest::RemovingOneOfTwoEntriesWithTheSamePathOffersNoUndo()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "First", .path = kSimlink});
    f.startup.entries.Carry(StartupEntry{.label = "Second", .path = kSimlink});

    QCOMPARE(f.editor.Remove(kProfileId, kSimlink).result, FileResult::Completed);

    QCOMPARE(f.startup.entries.Entries().size(), std::size_t{1});
    QVERIFY(!f.editor.WhatUndoWouldDo(kProfileId).has_value());
}

void StartupEditorTest::WithNothingToUndoThereIsNoOutcome()
{
    Fixture f;

    QVERIFY(!f.editor.WhatUndoWouldDo(kProfileId).has_value());
    QVERIFY(!f.editor.Undo(kProfileId).has_value());
    QVERIFY(f.journal.appended.empty());
}

void StartupEditorTest::TheRemovedListComesFromTheServiceNewestFirst()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "First", .path = kSimlink});
    f.startup.entries.Carry(StartupEntry{.label = "Second", .path = kAny2Gsx});
    QCOMPARE(f.editor.Remove(kProfileId, kSimlink).result, FileResult::Completed);
    f.clock.now += std::chrono::minutes(5);
    QCOMPARE(f.editor.Remove(kProfileId, kAny2Gsx).result, FileResult::Completed);

    const std::vector<StartupRemovedEntry> removed = f.editor.Removed();

    QCOMPARE(removed.size(), std::size_t{2});
    QCOMPARE(removed[0].entry.label, std::string("Second"));
    QCOMPARE(removed[1].entry.label, std::string("First"));
    QCOMPARE(removed[0].removedAt - removed[1].removedAt, std::chrono::system_clock::duration(std::chrono::minutes(5)));

    f.startup.service.Manage(false);

    QVERIFY(f.editor.Removed().empty());
}

QTEST_APPLESS_MAIN(StartupEditorTest)

#include "tst_startup_editor.moc"
