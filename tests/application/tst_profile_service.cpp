#include <QtTest/QtTest>

#include "domain/journal/JournalEntries.h"
#include "domain/journal/OperationLog.h"
#include "application/ProfileService.h"
#include "domain/linking/RepairPlan.h"
#include "domain/tree/EffectiveDestination.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "domain/importing/ExternalSidecar.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class ProfileServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheRecordBesideAnAddonIsReadOnEveryScanAndNotOnlyOnRegistration();
        static void TheRecordBesideTheAddonWinsOverTheOneInTheSettings();
        static void MarkingACategoryEnablesEveryAddonUnderIt();
        static void ABatchKeepsGoingAfterAFailureAndReportsOneResultPerItem();
        static void AFailedItemInABatchDoesNotUndoTheItemsThatWorked();
        static void AlreadyEnabledAddonsAreLeftAloneInsteadOfReportedAsOccupied();
        static void DisablingAnAddonRemovesItsLinkInEveryDestination();
        static void UndoingTheDisableOfALinkNamedDifferentlyFromItsFolderPutsItBackUnderItsOwnName();
        static void EnablingHonoursTheDestinationOverrideOfTheCategory();
        static void TurningAnAddonOffAndOnAgainLeavesItInTheDestinationItLivedIn();
        static void EveryLinkOperationReachesTheJournalWhetherItWorkedOrNot();
        static void UndoRevertsTheLastBatchAndNothingElse();
        static void UndoOnlyRevertsWhatTheBatchActuallyDid();
        static void ThereIsNothingToUndoBeforeTheFirstBatch();
        static void RegisteringALibraryReportsWhatIsInsideAndRefusesANestedFolder();
        static void ABatchWithNothingToDoDoesNotThrowAwayThePreviousUndo();
        static void ABatchWhereEveryStepFailedKeepsThePreviousUndo();
        static void RepairingRemovesTheDeadNodeAndJournalsIt();
        static void RepointingReplacesTheDeadLinkWithTheLibraryAddon();
        static void UndoingARepairRecreatesTheDeadLink();
        static void UndoingARepointRestoresTheDeadLink();
        static void ARepairLeavesAPlaceAloneWhoseDeadTargetCameBack();
        static void ARepairLeavesAPlaceAloneThatNowPointsAtAnotherDeadTarget();
        static void ARepairRunsTheRequestThatHoldsAndCountsTheOneThatDrifted();
        static void RepairingReadsTheDestinationsOnceAndDerivesTheEntriesAfterwards();
        static void ForgettingTheUndoLeavesTheLinksInPlaceAndOnlyDropsTheBatch();
        static void ABatchThatTurnsSomeOffAndOthersOnUndoesAsOnePiece();
        static void TheDisablesRunBeforeTheEnablesSoTheDestinationIsFree();
        static void UndoingASwapPutsTheOldAddonBackInTheDestination();
        static void EnablingAnAddonWhoseLinkVanishedAfterTheScanCreatesItAgain();
        static void ABatchWithNothingToDoSaysNothingDrifted();
        static void DisablingAnAddonWhoseLinkVanishedAfterTheScanReportsTheDrift();
        static void TheSwapPlansTheEnableEvenWhenTheScanThoughtTheAddonWasAlreadyOn();
        static void ThePlaceAnAddonWantsNamesTheAddonOfYoursHoldingIt();
        static void APlaceHeldByAFolderOrByAForeignLinkIsNotOfferedForSwapping();
        static void ThePlaceAnAddonWantsIsFoundFromItsPlannedPathAlone();
        static void AnAddonStandingInItsOwnPlaceIsNotAskedToSwapWithItself();
        static void APlaceHeldByAFolderAForeignLinkOrADeadLinkIsNotTakenByAnAddonOfYours();
        static void TheEntriesAfterABatchAreWhatAFullReadOfTheSameDiskGives();
        static void ASwapThatFailedHalfwayIsUndoneBackToTheAddonThatWasOn();
        static void TheAddonBeingDisabledNamesTheStartupEntryItCarries();
        static void AgreeingTurnsBothOffAsOneOperationAndUndoPutsBothBack();
        static void AStartupStepThatCouldNotWriteIsAFailureAndIsNotUndone();
        static void RefusingDisablesTheAddonAndLeavesTheEntryAndTheJournalAlone();
        static void WithTheStartupManagementOffNothingIsAskedAndTheFileIsNeverRead();
        static void ADisabledEntryInsideTheAddonIsNotWorthAsking();
        static void AnEntryInsideADestinationButOutsideTheAddonIsNotWorthAsking();
        static void TurningOffAStartupEntryOutsideTheAddonsCarriesItsLabelToTheJournal();
        static void TheStartupStepsOfABatchShareOneBackupRecord();
        static void UndoKeepsABackupRecordOfItsOwn();
        static void ALinkToTheOtherProgramsFolderWhoseCopyIsKnownOnlyBySidecarIsReadAsDivergent();
        static void ThePlaceTakenByThatLinkIsSeenByThePlacesTakenReadThroughTheLibraries();
        static void ThePlaceTakenByThatLinkIsSeenByThePlacesTakenNowRead();
        static void PlanningOverPlacesThatAreFreeReadsNoSidecar();
        static void PlanningReadsEachPlaceOnceEvenWhenSomeAreTaken();
        static void ThePlaceTakenIsTheEffectiveDestinationWithAndWithoutAnOverride();
        static void EnablingReadsTheDestinationsOnceAndDerivesTheEntriesAfterwards();
        static void RelinkingReadsTheDestinationsOnceAndDerivesTheEntriesAfterwards();
        static void UndoingReadsTheDestinationsOnceAndDerivesTheEntriesAfterwards();
        static void TheOutcomeOfABatchCarriesTheStampItWasAskedFor();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kCommunity2024 = "E:/Flight Simulator 2024/Community2024";
    constexpr auto kLibraryId = "library-1";
    const std::filesystem::path kPmdgLoader =
        std::filesystem::path(kCommunity) / "pmdg-aircraft-77w" / "bin" / "loader.exe";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

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

    TreeNode LibraryTree()
    {
        TreeNode node = CategoryNode(kLibrary,
                                     {CategoryNode("D:/MSFS 2024/Aircrafts",
                                                   {AddonNode("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"),
                                                    AddonNode("D:/MSFS 2024/Aircrafts/aerosoft-crj"),
                                                    AddonNode("D:/MSFS 2024/Aircrafts/fenix-a320")})});
        node.kind = TreeNodeKind::Library;

        return node;
    }

    SimulatorProfile Profile(std::vector<DestinationOverride> overrides = {})
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity, kCommunity2024};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = kLibraryId, .path = kLibrary, .label = "MSFS 2024"}};
        profile.destinationOverrides = std::move(overrides);

        return profile;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory(kCommunity2024);
            fileSystem.AddDirectory("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
            fileSystem.AddDirectory("D:/MSFS 2024/Aircrafts/aerosoft-crj");
            fileSystem.AddDirectory("D:/MSFS 2024/Aircrafts/fenix-a320");
            catalog.SetTree(kLibrary, LibraryTree());
        }

        [[nodiscard]] ProfileSnapshot Snapshot(const SimulatorProfile& profile) const
        {
            return service.Scan(profile);
        }

        [[nodiscard]] static std::vector<const TreeNode*> Aircrafts(const ProfileSnapshot& snapshot)
        {
            return {&snapshot.libraries.front().children.front()};
        }

        [[nodiscard]] static const TreeNode* AddonAt(const ProfileSnapshot& snapshot, const std::size_t index)
        {
            return &snapshot.libraries.front().children.front().children[index];
        }

        [[nodiscard]] std::vector<const TreeNode*> Pmdg(const ProfileSnapshot& snapshot) const
        {
            return {AddonAt(snapshot, 0)};
        }

        void CarryThreeStrangers()
        {
            for (const char* name : {"one", "two", "three"})
            {
                const std::filesystem::path path = std::filesystem::path("C:/Program Files/Other") / name / "agent.exe";
                fileSystem.AddFile(path);
                startup.entries.Carry(StartupEntry{.label = name, .path = path, .enabled = true});
            }
        }

        [[nodiscard]] static LinkBatch TurningOffTheThreeStrangers()
        {
            LinkBatch batch;
            for (const char* name : {"one", "two", "three"})
            {
                const StartupLine line{.label = name,
                                       .path = std::filesystem::path("C:/Program Files/Other") / name / "agent.exe",
                                       .enabled = true,
                                       .reach = StartupReach::OutsideYourAddons};
                batch.startupSwitches.push_back(StartupSwitch{.line = line, .enable = false});
            }

            return batch;
        }

        void CarryTheEntry(const bool enabled)
        {
            fileSystem.AddFile(kPmdgLoader);
            startup.entries.Carry(
                StartupEntry{.label = "PMDG Operations Center", .path = kPmdgLoader, .enabled = enabled});
        }

        InMemoryFileSystem fileSystem;

        FakeSidecarStore sidecars{fileSystem};
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
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
    };
}

void ProfileServiceTest::MarkingACategoryEnablesEveryAddonUnderIt()
{
    Fixture f;
    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true).results;

    QCOMPARE(results.size(), std::size_t{3});
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/aerosoft-crj"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/fenix-a320"));
}

void ProfileServiceTest::ABatchKeepsGoingAfterAFailureAndReportsOneResultPerItem()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true).results;

    QCOMPARE(results.size(), std::size_t{3});
    QCOMPARE(results.front().outcome.Failure(), LinkFailure::DestinationHoldsRealFolder);
    QVERIFY(results[1].outcome.Succeeded());
    QVERIFY(results[2].outcome.Succeeded());
}

void ProfileServiceTest::AFailedItemInABatchDoesNotUndoTheItemsThatWorked()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true).results;

    QCOMPARE(results.size(), std::size_t{3});
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/aerosoft-crj"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/fenix-a320"));
    QVERIFY(f.fileSystem.IsDirectory("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
}

void ProfileServiceTest::AlreadyEnabledAddonsAreLeftAloneInsteadOfReportedAsOccupied()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true).results;

    QCOMPARE(results.size(), std::size_t{2});
    for (const LinkOperationResult& result : results)
    {
        QVERIFY(result.outcome.Succeeded());
    }
}

void ProfileServiceTest::DisablingAnAddonRemovesItsLinkInEveryDestination()
{
    const std::filesystem::path folder = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";

    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", folder);
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w", folder);

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, {Fixture::AddonAt(snapshot, 0)}, false).results;

    QCOMPARE(results.size(), std::size_t{2});
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsDirectory(folder));
}

void ProfileServiceTest::UndoingTheDisableOfALinkNamedDifferentlyFromItsFolderPutsItBackUnderItsOwnName()
{
    const std::filesystem::path folder = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";
    const std::filesystem::path renamed = "E:/Flight Simulator 2024/Community/renamed-link";

    Fixture f;
    f.fileSystem.AddLink(renamed, folder);

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> disabled =
        f.service.SetEnabled(profile, snapshot, {Fixture::AddonAt(snapshot, 0)}, false).results;

    QCOMPARE(disabled.size(), std::size_t{1});
    QVERIFY(!f.fileSystem.Exists(renamed));

    const std::vector<LinkOperationResult> reverted = f.service.UndoLastBatch();

    QCOMPARE(reverted.size(), std::size_t{1});
    QVERIFY(f.fileSystem.IsLink(renamed));
    QCOMPARE(f.fileSystem.LinkTarget(renamed).value_or(std::filesystem::path{}), folder);
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
}

void ProfileServiceTest::EnablingHonoursTheDestinationOverrideOfTheCategory()
{
    Fixture f;
    const SimulatorProfile profile =
        Profile({{.libraryId = kLibraryId, .relativePath = "Aircrafts", .destination = kCommunity2024}});
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, {Fixture::AddonAt(snapshot, 0)}, true).results;

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().linkPath,
             std::filesystem::path("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w"));
}

void ProfileServiceTest::TurningAnAddonOffAndOnAgainLeavesItInTheDestinationItLivedIn()
{
    Fixture f;
    const SimulatorProfile profile =
        Profile({{.libraryId = kLibraryId, .relativePath = "Aircrafts", .destination = kCommunity2024}});

    const ProfileSnapshot enabled = f.Snapshot(profile);
    QCOMPARE(f.service.SetEnabled(profile, enabled, {Fixture::AddonAt(enabled, 1)}, true).results.size(),
             std::size_t{1});
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community2024/aerosoft-crj"));

    const ProfileSnapshot afterEnabling = f.Snapshot(profile);
    QCOMPARE(f.service.SetEnabled(profile, afterEnabling, {Fixture::AddonAt(afterEnabling, 1)}, false).results.size(),
             std::size_t{1});
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community2024/aerosoft-crj"));

    const ProfileSnapshot afterDisabling = f.Snapshot(profile);
    const std::vector<LinkOperationResult> again =
        f.service.SetEnabled(profile, afterDisabling, {Fixture::AddonAt(afterDisabling, 1)}, true).results;

    QCOMPARE(again.size(), std::size_t{1});
    QCOMPARE(again.front().linkPath, std::filesystem::path("E:/Flight Simulator 2024/Community2024/aerosoft-crj"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community2024/aerosoft-crj"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/aerosoft-crj"));
}

void ProfileServiceTest::EveryLinkOperationReachesTheJournalWhetherItWorkedOrNot()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true).results;

    QCOMPARE(results.size(), std::size_t{3});
    QCOMPARE(f.journal.appended.size(), std::size_t{3});

    const OperationRecord& failed = f.journal.appended.front();
    QCOMPARE(failed.kind, OperationKind::EnableAddon);
    QCOMPARE(std::get<LinkFailure>(failed.outcome), LinkFailure::DestinationHoldsRealFolder);
    QCOMPARE(failed.addonId, (AddonId{kLibraryId, "pmdg-aircraft-77w"}));
    QCOMPARE(failed.source, std::filesystem::path("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"));
    QCOMPARE(failed.target, std::filesystem::path("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QCOMPARE(failed.timestamp, f.clock.now);

    QCOMPARE(std::get<LinkFailure>(f.journal.appended[1].outcome), LinkFailure::None);
}

void ProfileServiceTest::UndoRevertsTheLastBatchAndNothingElse()
{
    Fixture f;
    const SimulatorProfile profile = Profile();

    const ProfileSnapshot first = f.Snapshot(profile);
    const std::vector<LinkOperationResult> kept =
        f.service.SetEnabled(profile, first, {Fixture::AddonAt(first, 0), Fixture::AddonAt(first, 1)}, true).results;

    const ProfileSnapshot second = f.Snapshot(profile);
    const std::vector<LinkOperationResult> undone =
        f.service.SetEnabled(profile, second, {Fixture::AddonAt(second, 2)}, true).results;

    QCOMPARE(kept.size(), std::size_t{2});
    QCOMPARE(undone.size(), std::size_t{1});
    QVERIFY(f.service.CanUndo());

    const std::vector<LinkOperationResult> reverted = f.service.UndoLastBatch();

    QCOMPARE(reverted.size(), std::size_t{1});
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/fenix-a320"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/aerosoft-crj"));
    QVERIFY(!f.service.CanUndo());
}

void ProfileServiceTest::UndoOnlyRevertsWhatTheBatchActuallyDid()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true).results;
    const std::vector<LinkOperationResult> reverted = f.service.UndoLastBatch();

    QCOMPARE(results.size(), std::size_t{3});
    QCOMPARE(reverted.size(), std::size_t{2});
    QVERIFY(f.fileSystem.IsDirectory("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/aerosoft-crj"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/fenix-a320"));
}

void ProfileServiceTest::ThereIsNothingToUndoBeforeTheFirstBatch()
{
    const Fixture f;

    QVERIFY(!f.service.CanUndo());
}

void ProfileServiceTest::RegisteringALibraryReportsWhatIsInsideAndRefusesANestedFolder()
{
    const Fixture f;
    SimulatorProfile profile = Profile();
    profile.libraries.clear();

    const LibraryReport accepted = f.service.RegisterLibrary(profile, kLibrary);

    QVERIFY(accepted.Accepted());
    QCOMPARE(accepted.categories, std::size_t{1});
    QCOMPARE(accepted.addons, std::size_t{3});
    QCOMPARE(profile.libraries.size(), std::size_t{1});

    const LibraryReport nested = f.service.RegisterLibrary(profile, "D:/MSFS 2024/Aircrafts");

    QVERIFY(!nested.Accepted());
    QCOMPARE(nested.addons, std::size_t{0});
    QCOMPARE(profile.libraries.size(), std::size_t{1});
}

void ProfileServiceTest::ABatchWithNothingToDoDoesNotThrowAwayThePreviousUndo()
{
    Fixture f;
    const SimulatorProfile profile = Profile();

    const ProfileSnapshot before = f.Snapshot(profile);
    const std::vector<LinkOperationResult> done =
        f.service.SetEnabled(profile, before, {Fixture::AddonAt(before, 0)}, true).results;

    const ProfileSnapshot after = f.Snapshot(profile);
    const std::vector<LinkOperationResult> again =
        f.service.SetEnabled(profile, after, {Fixture::AddonAt(after, 0)}, true).results;

    QCOMPARE(done.size(), std::size_t{1});
    QVERIFY(again.empty());
    QVERIFY(f.service.CanUndo());

    QCOMPARE(f.service.UndoLastBatch().size(), std::size_t{1});
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
}

void ProfileServiceTest::ABatchWhereEveryStepFailedKeepsThePreviousUndo()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/aerosoft-crj");

    const SimulatorProfile profile = Profile();

    const ProfileSnapshot before = f.Snapshot(profile);
    const std::vector<LinkOperationResult> done =
        f.service.SetEnabled(profile, before, {Fixture::AddonAt(before, 0)}, true).results;

    const ProfileSnapshot after = f.Snapshot(profile);
    const std::vector<LinkOperationResult> failed =
        f.service.SetEnabled(profile, after, {Fixture::AddonAt(after, 1)}, true).results;

    QCOMPARE(done.size(), std::size_t{1});
    QCOMPARE(failed.size(), std::size_t{1});
    QCOMPARE(failed.front().outcome.Failure(), LinkFailure::DestinationHoldsRealFolder);
    QVERIFY(f.service.CanUndo());

    QCOMPARE(f.service.UndoLastBatch().size(), std::size_t{1});
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
}

namespace
{
    std::vector<RepairRequest> Requests(const Fixture& f, const SimulatorProfile& profile, const RepairAction action)
    {
        const ProfileSnapshot snapshot = f.Snapshot(profile);

        std::vector<RepairRequest> requests;
        for (const RepairCandidate& candidate : PlanRepairs(profile, snapshot.entries, snapshot.libraries))
        {
            requests.push_back({.candidate = candidate, .action = action});
        }

        return requests;
    }

    LinkBatchOutcome RepairNow(Fixture& f, const SimulatorProfile& profile, const std::vector<RepairRequest>& requests)
    {
        return f.service.Repair(EntriesStamp{.profile = profile}, f.Snapshot(profile).libraries, requests);
    }
}

void ProfileServiceTest::RepairingRemovesTheDeadNodeAndJournalsIt()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/gone", "D:/Removed Library/gone");

    const SimulatorProfile profile = Profile();
    const std::vector<LinkOperationResult> results =
        RepairNow(f, profile, Requests(f, profile, RepairAction::RemoveDeadNode)).report.results;

    QCOMPARE(results.size(), std::size_t{1});
    QVERIFY(results.front().outcome.Succeeded());
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/gone"));
    QVERIFY(f.fileSystem.IsDirectory("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"));

    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(f.journal.appended.front().kind, OperationKind::RemoveBrokenLink);
    QCOMPARE(f.journal.appended.front().target, std::filesystem::path("E:/Flight Simulator 2024/Community/gone"));
}

void ProfileServiceTest::RepointingReplacesTheDeadLinkWithTheLibraryAddon()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", "D:/Old Library/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const std::vector<LinkOperationResult> results =
        RepairNow(f, profile, Requests(f, profile, RepairAction::Repoint)).report.results;

    QCOMPARE(results.size(), std::size_t{1});
    QVERIFY(results.front().outcome.Succeeded());
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QCOMPARE(f.fileSystem.LinkTarget("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"),
             std::optional<std::filesystem::path>("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"));
    QCOMPARE(f.journal.appended.front().kind, OperationKind::RepointLink);
}

void ProfileServiceTest::UndoingARepairRecreatesTheDeadLink()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/gone", "D:/Removed Library/gone");

    const SimulatorProfile profile = Profile();
    const std::vector<LinkOperationResult> repaired =
        RepairNow(f, profile, Requests(f, profile, RepairAction::RemoveDeadNode)).report.results;

    QCOMPARE(repaired.size(), std::size_t{1});
    QVERIFY(f.service.CanUndo());

    const std::vector<LinkOperationResult> reverted = f.service.UndoLastBatch();

    QCOMPARE(reverted.size(), std::size_t{1});
    QCOMPARE(f.fileSystem.LinkTarget("E:/Flight Simulator 2024/Community/gone"),
             std::optional<std::filesystem::path>("D:/Removed Library/gone"));
}

void ProfileServiceTest::UndoingARepointRestoresTheDeadLink()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", "D:/Old Library/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const std::vector<LinkOperationResult> repaired =
        RepairNow(f, profile, Requests(f, profile, RepairAction::Repoint)).report.results;

    QCOMPARE(repaired.size(), std::size_t{1});

    const std::vector<LinkOperationResult> reverted = f.service.UndoLastBatch();

    QCOMPARE(reverted.size(), std::size_t{2});
    QCOMPARE(f.fileSystem.LinkTarget("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"),
             std::optional<std::filesystem::path>("D:/Old Library/pmdg-aircraft-77w"));
}

namespace
{
    constexpr auto kGone = "E:/Flight Simulator 2024/Community/gone";
    constexpr auto kLost = "E:/Flight Simulator 2024/Community/lost";

    void EnableThePmdgSoThereIsSomethingToUndo(Fixture& f, const SimulatorProfile& profile)
    {
        const ProfileSnapshot shown = f.Snapshot(profile);

        QCOMPARE(f.service.SetEnabled(profile, shown, {Fixture::AddonAt(shown, 0)}, true).results.size(),
                 std::size_t{1});
        QVERIFY(f.service.CanUndo());
    }

    void VerifyTheRepairLeftTheUndoAndTheJournalAsTheyWere(Fixture& f, const std::size_t journalSize)
    {
        QCOMPARE(f.journal.appended.size(), journalSize);
        QVERIFY(f.service.CanUndo());
        QCOMPARE(f.service.UndoLastBatch().size(), std::size_t{1});
        QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    }
}

void ProfileServiceTest::ARepairLeavesAPlaceAloneWhoseDeadTargetCameBack()
{
    Fixture f;
    f.fileSystem.AddLink(kGone, "D:/Removed Library/gone");

    const SimulatorProfile profile = Profile();
    EnableThePmdgSoThereIsSomethingToUndo(f, profile);
    const std::size_t journalSize = f.journal.appended.size();

    const std::vector<RepairRequest> requests = Requests(f, profile, RepairAction::RemoveDeadNode);
    QCOMPARE(requests.size(), std::size_t{1});

    f.fileSystem.AddDirectory("D:/Removed Library/gone");

    const LinkBatchOutcome outcome = RepairNow(f, profile, requests);

    QVERIFY(outcome.report.results.empty());
    QCOMPARE(outcome.report.drifted, std::size_t{1});
    QVERIFY(f.fileSystem.IsLink(kGone));
    QCOMPARE(f.fileSystem.LinkTarget(kGone), std::optional<std::filesystem::path>("D:/Removed Library/gone"));

    VerifyTheRepairLeftTheUndoAndTheJournalAsTheyWere(f, journalSize);
}

void ProfileServiceTest::ARepairLeavesAPlaceAloneThatNowPointsAtAnotherDeadTarget()
{
    Fixture f;
    f.fileSystem.AddLink(kGone, "D:/Removed Library/gone");

    const SimulatorProfile profile = Profile();
    EnableThePmdgSoThereIsSomethingToUndo(f, profile);
    const std::size_t journalSize = f.journal.appended.size();

    const std::vector<RepairRequest> requests = Requests(f, profile, RepairAction::RemoveDeadNode);
    QCOMPARE(requests.size(), std::size_t{1});

    f.fileSystem.AddLink(kGone, "D:/Another Library/gone");

    const LinkBatchOutcome outcome = RepairNow(f, profile, requests);

    QVERIFY(outcome.report.results.empty());
    QCOMPARE(outcome.report.drifted, std::size_t{1});
    QCOMPARE(f.fileSystem.LinkTarget(kGone), std::optional<std::filesystem::path>("D:/Another Library/gone"));

    VerifyTheRepairLeftTheUndoAndTheJournalAsTheyWere(f, journalSize);
}

void ProfileServiceTest::ARepairRunsTheRequestThatHoldsAndCountsTheOneThatDrifted()
{
    Fixture f;
    f.fileSystem.AddLink(kGone, "D:/Removed Library/gone");
    f.fileSystem.AddLink(kLost, "D:/Removed Library/lost");

    const SimulatorProfile profile = Profile();
    const std::vector<RepairRequest> requests = Requests(f, profile, RepairAction::RemoveDeadNode);
    QCOMPARE(requests.size(), std::size_t{2});

    f.fileSystem.AddDirectory("D:/Removed Library/lost");

    const LinkBatchOutcome outcome = RepairNow(f, profile, requests);

    QCOMPARE(outcome.report.results.size(), std::size_t{1});
    QCOMPARE(outcome.report.results.front().linkPath, std::filesystem::path(kGone));
    QCOMPARE(outcome.report.drifted, std::size_t{1});
    QVERIFY(!f.fileSystem.Exists(kGone));
    QVERIFY(f.fileSystem.IsLink(kLost));
    QCOMPARE(f.journal.appended.size(), std::size_t{1});

    QCOMPARE(f.service.UndoLastBatch().size(), std::size_t{1});
    QCOMPARE(f.fileSystem.LinkTarget(kGone), std::optional<std::filesystem::path>("D:/Removed Library/gone"));
    QCOMPARE(f.fileSystem.LinkTarget(kLost), std::optional<std::filesystem::path>("D:/Removed Library/lost"));
}

void ProfileServiceTest::RepairingReadsTheDestinationsOnceAndDerivesTheEntriesAfterwards()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/gone", "D:/Removed Library/gone");
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/aerosoft-crj", "D:/MSFS 2024/Aircrafts/aerosoft-crj");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot shown = f.Snapshot(profile);
    const std::vector<RepairRequest> requests = Requests(f, profile, RepairAction::RemoveDeadNode);
    f.linkService.ForgetTheReads();

    const LinkBatchOutcome outcome = f.service.Repair(EntriesStamp{.profile = profile}, shown.libraries, requests);

    QCOMPARE(outcome.report.results.size(), std::size_t{1});
    QCOMPARE(f.linkService.TimesRead("E:/Flight Simulator 2024/Community/gone"), std::size_t{1});
    QCOMPARE(f.linkService.TimesRead("E:/Flight Simulator 2024/Community/aerosoft-crj"), std::size_t{1});
    QCOMPARE(f.linkService.PlacesRead(), std::size_t{2});
    QCOMPARE(outcome.read.entries.size(), std::size_t{1});
    QCOMPARE(outcome.read.entries.front().path,
             std::filesystem::path{"E:/Flight Simulator 2024/Community/aerosoft-crj"});
}

void ProfileServiceTest::ForgettingTheUndoLeavesTheLinksInPlaceAndOnlyDropsTheBatch()
{
    Fixture f;
    const SimulatorProfile profile = Profile();

    const ProfileSnapshot snapshot = f.Snapshot(profile);
    QCOMPARE(f.service.SetEnabled(profile, snapshot, {Fixture::AddonAt(snapshot, 0)}, true).results.size(),
             std::size_t{1});
    QVERIFY(f.service.CanUndo());

    f.service.ForgetUndo();

    QVERIFY(!f.service.CanUndo());
    QVERIFY(f.service.UndoLastBatch().empty());
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
}

void ProfileServiceTest::ABatchThatTurnsSomeOffAndOthersOnUndoesAsOnePiece()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service
            .SetEnabled(
                profile, snapshot,
                LinkBatch{.toDisable = {Fixture::AddonAt(snapshot, 0)}, .toEnable = {Fixture::AddonAt(snapshot, 1)}})
            .results;

    QCOMPARE(results.size(), std::size_t{2});
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/aerosoft-crj"));

    const std::vector<LinkOperationResult> reverted = f.service.UndoLastBatch();

    QCOMPARE(reverted.size(), std::size_t{2});
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/aerosoft-crj"));
}

void ProfileServiceTest::TheDisablesRunBeforeTheEnablesSoTheDestinationIsFree()
{
    Fixture f;
    f.fileSystem.AddDirectory("F:/Extra");
    f.fileSystem.AddDirectory("F:/Extra/pmdg-aircraft-77w");
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    TreeNode extra = CategoryNode("F:/Extra", {AddonNode("F:/Extra/pmdg-aircraft-77w")});
    extra.kind = TreeNodeKind::Library;
    f.catalog.SetTree("F:/Extra", extra);

    SimulatorProfile profile = Profile();
    profile.libraries.push_back(Library{.id = "library-2", .path = "F:/Extra", .label = "Extra"});

    const ProfileSnapshot snapshot = f.Snapshot(profile);
    const TreeNode* replacement = &snapshot.libraries[1].children.front();

    const std::vector<LinkOperationResult> results =
        f.service
            .SetEnabled(profile, snapshot,
                        LinkBatch{.toDisable = {Fixture::AddonAt(snapshot, 0)}, .toEnable = {replacement}})
            .results;

    QCOMPARE(results.size(), std::size_t{2});
    for (const LinkOperationResult& result : results)
    {
        QVERIFY(result.outcome.Succeeded());
    }

    QCOMPARE(f.fileSystem.LinkTarget("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"),
             std::optional<std::filesystem::path>{"F:/Extra/pmdg-aircraft-77w"});
}

void ProfileServiceTest::UndoingASwapPutsTheOldAddonBackInTheDestination()
{
    Fixture f;
    f.fileSystem.AddDirectory("F:/Extra");
    f.fileSystem.AddDirectory("F:/Extra/pmdg-aircraft-77w");
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    TreeNode extra = CategoryNode("F:/Extra", {AddonNode("F:/Extra/pmdg-aircraft-77w")});
    extra.kind = TreeNodeKind::Library;
    f.catalog.SetTree("F:/Extra", extra);

    SimulatorProfile profile = Profile();
    profile.libraries.push_back(Library{.id = "library-2", .path = "F:/Extra", .label = "Extra"});

    const ProfileSnapshot snapshot = f.Snapshot(profile);
    const TreeNode* replacement = &snapshot.libraries[1].children.front();

    QCOMPARE(f.service.SetEnabled(profile, snapshot, LinkBatch{{Fixture::AddonAt(snapshot, 0)}, {replacement}})
                 .results.size(),
             std::size_t{2});

    const std::vector<LinkOperationResult> reverted = f.service.UndoLastBatch();

    QCOMPARE(reverted.size(), std::size_t{2});
    for (const LinkOperationResult& result : reverted)
    {
        QVERIFY(result.outcome.Succeeded());
    }

    QCOMPARE(f.fileSystem.LinkTarget("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"),
             std::optional<std::filesystem::path>{"D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"});
}

void ProfileServiceTest::EnablingAnAddonWhoseLinkVanishedAfterTheScanCreatesItAgain()
{
    const std::filesystem::path link = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w";

    Fixture f;
    f.fileSystem.AddLink(link, "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot shown = f.Snapshot(profile);
    QVERIFY(shown.enabled.Contains("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"));

    QVERIFY(f.fileSystem.RemoveNode(link));

    const LinkBatchReport report = f.service.SetEnabled(profile, shown, {Fixture::AddonAt(shown, 0)}, true);

    QCOMPARE(report.results.size(), std::size_t{1});
    QVERIFY(report.results.front().outcome.Succeeded());
    QCOMPARE(report.drifted, std::size_t{1});
    QVERIFY(f.fileSystem.IsLink(link));
    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(f.journal.appended.front().kind, OperationKind::EnableAddon);
    QCOMPARE(f.journal.appended.front().target, link);
}

void ProfileServiceTest::ABatchWithNothingToDoSaysNothingDrifted()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot shown = f.Snapshot(profile);

    const LinkBatchReport report = f.service.SetEnabled(profile, shown, {Fixture::AddonAt(shown, 0)}, true);

    QVERIFY(report.results.empty());
    QCOMPARE(report.drifted, std::size_t{0});
    QVERIFY(f.journal.appended.empty());
}

void ProfileServiceTest::DisablingAnAddonWhoseLinkVanishedAfterTheScanReportsTheDrift()
{
    const std::filesystem::path link = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w";

    Fixture f;
    f.fileSystem.AddLink(link, "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot shown = f.Snapshot(profile);

    QVERIFY(f.fileSystem.RemoveNode(link));

    const LinkBatchReport report = f.service.SetEnabled(profile, shown, {Fixture::AddonAt(shown, 0)}, false);

    QVERIFY(report.results.empty());
    QCOMPARE(report.drifted, std::size_t{1});
    QVERIFY(f.journal.appended.empty());
}

void ProfileServiceTest::TheSwapPlansTheEnableEvenWhenTheScanThoughtTheAddonWasAlreadyOn()
{
    const std::filesystem::path vanished = "E:/Flight Simulator 2024/Community/aerosoft-crj";

    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
    f.fileSystem.AddLink(vanished, "D:/MSFS 2024/Aircrafts/aerosoft-crj");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot shown = f.Snapshot(profile);

    QVERIFY(f.fileSystem.RemoveNode(vanished));

    const LinkBatchReport report = f.service.SetEnabled(
        profile, shown, LinkBatch{.toDisable = {Fixture::AddonAt(shown, 0)}, .toEnable = {Fixture::AddonAt(shown, 1)}});

    QCOMPARE(report.results.size(), std::size_t{2});
    QCOMPARE(report.drifted, std::size_t{1});
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsLink(vanished));
}

namespace
{
    SimulatorProfile ProfileWithASecondLibraryHolding(Fixture& f, const std::filesystem::path& addon)
    {
        f.fileSystem.AddDirectory("F:/Extra");
        f.fileSystem.AddDirectory(addon);

        TreeNode extra = CategoryNode("F:/Extra", {AddonNode(addon)});
        extra.kind = TreeNodeKind::Library;
        f.catalog.SetTree("F:/Extra", extra);

        SimulatorProfile profile = Profile();
        profile.libraries.push_back(Library{.id = "library-2", .path = "F:/Extra", .label = "Extra"});

        return profile;
    }
}

void ProfileServiceTest::ThePlaceAnAddonWantsNamesTheAddonOfYoursHoldingIt()
{
    Fixture f;
    const std::filesystem::path place = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w";
    const SimulatorProfile profile = ProfileWithASecondLibraryHolding(f, "F:/Extra/pmdg-aircraft-77w");

    f.fileSystem.AddLink(place, "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    const ProfileSnapshot snapshot = f.Snapshot(profile);
    const TreeNode* wanted = &snapshot.libraries[1].children.front();

    const std::vector<TakenPlace> taken = f.service.PlacesTaken(profile, {wanted}, snapshot.libraries);

    QCOMPARE(taken.size(), std::size_t{1});
    QCOMPARE(taken.front().addonFolder, std::filesystem::path{"F:/Extra/pmdg-aircraft-77w"});
    QCOMPARE(taken.front().linkPath, place);
    QCOMPARE(taken.front().occupant, std::filesystem::path{"D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"});

    QVERIFY(f.service.PlacesTaken(profile, {Fixture::AddonAt(snapshot, 0)}, snapshot.libraries).empty());
}

void ProfileServiceTest::APlaceHeldByAFolderOrByAForeignLinkIsNotOfferedForSwapping()
{
    Fixture f;
    const std::filesystem::path place = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w";
    const SimulatorProfile profile = ProfileWithASecondLibraryHolding(f, "F:/Extra/pmdg-aircraft-77w");

    f.fileSystem.AddDirectory(place);

    const ProfileSnapshot withAFolder = f.Snapshot(profile);
    QVERIFY(
        f.service.PlacesTaken(profile, {&withAFolder.libraries[1].children.front()}, withAFolder.libraries).empty());

    QVERIFY(f.fileSystem.RemoveTree(place));
    f.fileSystem.AddDirectory("G:/Another program/pmdg-aircraft-77w");
    f.fileSystem.AddLink(place, "G:/Another program/pmdg-aircraft-77w");

    const ProfileSnapshot withAForeignLink = f.Snapshot(profile);
    QVERIFY(
        f.service.PlacesTaken(profile, {&withAForeignLink.libraries[1].children.front()}, withAForeignLink.libraries)
            .empty());

    QVERIFY(f.fileSystem.RemoveTree("G:/Another program/pmdg-aircraft-77w"));

    const ProfileSnapshot withADeadLink = f.Snapshot(profile);
    QVERIFY(f.service.PlacesTaken(profile, {&withADeadLink.libraries[1].children.front()}, withADeadLink.libraries)
                .empty());
}

void ProfileServiceTest::ThePlaceAnAddonWantsIsFoundFromItsPlannedPathAlone()
{
    Fixture f;
    const std::filesystem::path place = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w";
    const SimulatorProfile profile = ProfileWithASecondLibraryHolding(f, "F:/Extra/pmdg-aircraft-77w");

    f.fileSystem.AddLink(place, "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/aerosoft-crj", "D:/MSFS 2024/Aircrafts/aerosoft-crj");

    const ProfileSnapshot snapshot = f.Snapshot(profile);
    const TreeNode* wanted = &snapshot.libraries[1].children.front();

    f.filesystemProbe.enumerated.clear();

    const std::vector<TakenPlace> taken = f.service.PlacesTakenNow(profile, {wanted}, snapshot);

    QCOMPARE(taken.size(), std::size_t{1});
    QCOMPARE(taken.front().addonFolder, std::filesystem::path{"F:/Extra/pmdg-aircraft-77w"});
    QCOMPARE(taken.front().linkPath, place);
    QCOMPARE(taken.front().occupant, std::filesystem::path{"D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"});
    QVERIFY2(f.filesystemProbe.enumerated.empty(), "no destination is listed to answer about one place");

    QVERIFY(f.service.PlacesTakenNow(profile, {Fixture::AddonAt(snapshot, 0)}, snapshot).empty());
}

void ProfileServiceTest::AnAddonStandingInItsOwnPlaceIsNotAskedToSwapWithItself()
{
    Fixture f;
    const std::filesystem::path place = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w";
    const SimulatorProfile profile = Profile();
    const ProfileSnapshot shown = f.Snapshot(profile);

    f.fileSystem.AddLink(place, "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    QVERIFY(!shown.enabled.Contains("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"));
    QVERIFY(f.service.PlacesTakenNow(profile, {Fixture::AddonAt(shown, 0)}, shown).empty());

    QVERIFY(f.fileSystem.RemoveNode(place));
    f.fileSystem.AddLink(place, "D:/MSFS 2024/Aircrafts/aerosoft-crj");

    QCOMPARE(f.service.PlacesTakenNow(profile, {Fixture::AddonAt(shown, 0)}, shown).size(), std::size_t{1});
}

void ProfileServiceTest::APlaceHeldByAFolderAForeignLinkOrADeadLinkIsNotTakenByAnAddonOfYours()
{
    Fixture f;
    const std::filesystem::path place = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w";
    const SimulatorProfile profile = ProfileWithASecondLibraryHolding(f, "F:/Extra/pmdg-aircraft-77w");
    const ProfileSnapshot snapshot = f.Snapshot(profile);
    const std::vector<const TreeNode*> wanted{&snapshot.libraries[1].children.front()};

    f.fileSystem.AddDirectory(place);
    QVERIFY(f.service.PlacesTakenNow(profile, wanted, snapshot).empty());

    QVERIFY(f.fileSystem.RemoveTree(place));
    f.fileSystem.AddDirectory("G:/Another program/pmdg-aircraft-77w");
    f.fileSystem.AddLink(place, "G:/Another program/pmdg-aircraft-77w");
    QVERIFY(f.service.PlacesTakenNow(profile, wanted, snapshot).empty());

    QVERIFY(f.fileSystem.RemoveTree("G:/Another program/pmdg-aircraft-77w"));
    QVERIFY(f.service.PlacesTakenNow(profile, wanted, snapshot).empty());
}

void ProfileServiceTest::TheEntriesAfterABatchAreWhatAFullReadOfTheSameDiskGives()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/aerosoft-crj", "D:/MSFS 2024/Aircrafts/aerosoft-crj");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot shown = f.Snapshot(profile);
    const std::vector<ExternalAddon> externals = f.service.WhatCameFromAnotherProgram(profile, shown.libraries);
    const ProfileService::LinksOnDisk onDisk = f.service.ReadLinksNow(profile, externals);

    const LinkBatchReport report = f.service.SetEnabled(
        profile, shown, LinkBatch{.toDisable = {Fixture::AddonAt(shown, 1)}, .toEnable = {Fixture::AddonAt(shown, 2)}},
        onDisk);
    QCOMPARE(report.results.size(), std::size_t{2});

    const std::vector<DestinationEntry> incremental =
        f.service.EntriesAfter(profile, onDisk.entries, report.results, externals);
    const std::vector<DestinationEntry> full = f.service.ResolveEntries(profile, shown.libraries);

    QCOMPARE(incremental.size(), full.size());
    for (std::size_t index = 0; index < full.size(); ++index)
    {
        QCOMPARE(incremental[index].path, full[index].path);
        QCOMPARE(incremental[index].target, full[index].target);
        QCOMPARE(incremental[index].classification, full[index].classification);
    }
}

void ProfileServiceTest::ASwapThatFailedHalfwayIsUndoneBackToTheAddonThatWasOn()
{
    Fixture f;
    const std::filesystem::path place = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w";
    const SimulatorProfile profile = ProfileWithASecondLibraryHolding(f, "F:/Extra/pmdg-aircraft-77w");

    f.fileSystem.AddLink(place, "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    const ProfileSnapshot snapshot = f.Snapshot(profile);
    f.linkService.MakeLinkCreationFail();

    const LinkBatchReport report = f.service.SetEnabled(
        profile, snapshot,
        LinkBatch{.toDisable = {Fixture::AddonAt(snapshot, 0)}, .toEnable = {&snapshot.libraries[1].children.front()}});

    QCOMPARE(report.results.size(), std::size_t{2});
    QCOMPARE(report.results.front().kind, OperationKind::DisableAddon);
    QVERIFY(report.results.front().outcome.Succeeded());
    QCOMPARE(report.results.back().kind, OperationKind::EnableAddon);
    QCOMPARE(report.results.back().outcome.Failure(), LinkFailure::CouldNotCreateLink);
    QVERIFY(!f.fileSystem.Exists(place));

    QVERIFY(f.service.CanUndo());
    f.linkService.MakeLinkCreationFailWith(LinkFailure::None);

    const std::vector<LinkOperationResult> reverted = f.service.UndoLastBatch();

    QCOMPARE(reverted.size(), std::size_t{1});
    QVERIFY(reverted.front().outcome.Succeeded());
    QCOMPARE(f.fileSystem.LinkTarget(place),
             std::optional<std::filesystem::path>{"D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"});
}

namespace
{
    constexpr auto kVendorFolder = "C:/Program Files (x86)/Addon Manager/MSFS/gsx-pro";
    constexpr auto kImportedExternal = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";

    void PutTheOtherProgramsFolderBackOnDisk(Fixture& f)
    {
        f.fileSystem.AddDirectory("C:/Program Files (x86)/Addon Manager/MSFS");
        f.fileSystem.AddDirectory(kVendorFolder);
        f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", kImportedExternal);
    }
}

void ProfileServiceTest::TheRecordBesideAnAddonIsReadOnEveryScanAndNotOnlyOnRegistration()
{
    Fixture f;
    PutTheOtherProgramsFolderBackOnDisk(f);
    f.fileSystem.AddFileWithContents(ExternalSidecarPathFor(kImportedExternal), TextOfTheExternalOrigin(kVendorFolder));

    const SimulatorProfile forgetful = Profile();
    QVERIFY(forgetful.externalOrigins.empty());

    const ProfileSnapshot snapshot = f.Snapshot(forgetful);

    const auto entry = std::ranges::find_if(snapshot.entries,
                                            [](const DestinationEntry& candidate)
                                            {
                                                return candidate.classification == EntryClassification::Divergent;
                                            });

    QVERIFY(entry != snapshot.entries.end());
    QCOMPARE(entry->externalOrigin, std::filesystem::path{kVendorFolder});
}

void ProfileServiceTest::TheRecordBesideTheAddonWinsOverTheOneInTheSettings()
{
    const std::filesystem::path movedTo = "C:/Program Files (x86)/Addon Manager/MSFS/gsx-pro-2";

    Fixture f;
    PutTheOtherProgramsFolderBackOnDisk(f);
    f.fileSystem.AddDirectory(movedTo);
    f.fileSystem.AddFileWithContents(ExternalSidecarPathFor(kImportedExternal), TextOfTheExternalOrigin(movedTo));

    SimulatorProfile stale = Profile();
    stale.externalOrigins = {ExternalOrigin{
        .libraryId = kLibraryId, .relativePath = "Aircrafts/pmdg-aircraft-77w", .externalPath = kVendorFolder}};

    const ProfileSnapshot snapshot = f.Snapshot(stale);

    const auto entry = std::ranges::find_if(snapshot.entries,
                                            [](const DestinationEntry& candidate)
                                            {
                                                return candidate.classification == EntryClassification::Divergent;
                                            });

    QVERIFY(entry != snapshot.entries.end());
    QCOMPARE(entry->externalOrigin, movedTo);
}

void ProfileServiceTest::TheAddonBeingDisabledNamesTheStartupEntryItCarries()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
    f.CarryTheEntry(true);

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<StartupLine> carried = f.service.StartupEntriesCarriedBy(profile, snapshot, f.Pmdg(snapshot));

    QCOMPARE(carried.size(), std::size_t{1});
    QCOMPARE(carried.front().label, std::string("PMDG Operations Center"));
    QCOMPARE(carried.front().path, kPmdgLoader);
}

void ProfileServiceTest::AgreeingTurnsBothOffAsOneOperationAndUndoPutsBothBack()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
    f.CarryTheEntry(true);

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const LinkBatch batch{.toDisable = f.Pmdg(snapshot),
                          .toEnable = {},
                          .startupEntriesToTurnOff =
                              f.service.StartupEntriesCarriedBy(profile, snapshot, f.Pmdg(snapshot))};

    const std::vector<LinkOperationResult> results = f.service.SetEnabled(profile, snapshot, batch).results;

    QCOMPARE(results.size(), std::size_t{2});
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(!f.startup.entries.Entries().front().enabled);

    const std::vector<JournalEntry> written = GroupOperations(f.journal.Read());
    QCOMPARE(written.size(), std::size_t{1});
    QVERIFY(written.front().IsAnAddonAndItsStartupEntry());

    const std::vector<LinkOperationResult> reverted = f.service.UndoLastBatch();

    QCOMPARE(reverted.size(), std::size_t{2});
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(f.startup.entries.Entries().front().enabled);
}

void ProfileServiceTest::AStartupStepThatCouldNotWriteIsAFailureAndIsNotUndone()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
    f.CarryTheEntry(true);
    f.startup.entries.MakeSwitchingFailWith(FileResult::CouldNotWriteTheStartupFile);

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const LinkBatch batch{.toDisable = f.Pmdg(snapshot),
                          .toEnable = {},
                          .startupEntriesToTurnOff =
                              f.service.StartupEntriesCarriedBy(profile, snapshot, f.Pmdg(snapshot))};

    const std::vector<LinkOperationResult> results = f.service.SetEnabled(profile, snapshot, batch).results;

    QCOMPARE(results.size(), std::size_t{2});

    const auto startupStep =
        std::ranges::find(results, OperationKind::TurnOffTheStartupEntry, &LinkOperationResult::kind);

    QVERIFY(startupStep != results.end());
    QVERIFY(!startupStep->outcome.Succeeded());
    QVERIFY(f.startup.entries.Entries().front().enabled);

    const std::vector<LinkOperationResult> reverted = f.service.UndoLastBatch();

    QCOMPARE(reverted.size(), std::size_t{1});
    QCOMPARE(reverted.front().kind, OperationKind::EnableAddon);
}

void ProfileServiceTest::RefusingDisablesTheAddonAndLeavesTheEntryAndTheJournalAlone()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
    f.CarryTheEntry(true);

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, LinkBatch{.toDisable = f.Pmdg(snapshot)}).results;

    QCOMPARE(results.size(), std::size_t{1});
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(f.startup.entries.Entries().front().enabled);
    QCOMPARE(f.startup.entries.writes, std::size_t{0});

    for (const OperationRecord& record : f.journal.Read())
    {
        QVERIFY(record.kind != OperationKind::TurnOffTheStartupEntry);
    }
}

void ProfileServiceTest::TurningOffAStartupEntryOutsideTheAddonsCarriesItsLabelToTheJournal()
{
    Fixture f;
    const std::filesystem::path stranger = "C:/Program Files/Other/agent.exe";
    f.fileSystem.AddFile(stranger);
    f.startup.entries.Carry(StartupEntry{.label = "Other launcher", .path = stranger, .enabled = true});

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const StartupLine line{
        .label = "Other launcher", .path = stranger, .enabled = true, .reach = StartupReach::OutsideYourAddons};
    const LinkBatch batch{.startupSwitches = {StartupSwitch{.line = line, .enable = false}}};

    const std::vector<LinkOperationResult> results = f.service.SetEnabled(profile, snapshot, batch).results;

    QCOMPARE(results.size(), std::size_t{1});
    QVERIFY(results.front().outcome.Succeeded());

    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    const OperationRecord& record = f.journal.appended.front();
    QCOMPARE(record.kind, OperationKind::TurnOffTheStartupEntry);
    QVERIFY(record.addonId.folderName.empty());
    QCOMPARE(record.label, std::string("Other launcher"));
}

void ProfileServiceTest::WithTheStartupManagementOffNothingIsAskedAndTheFileIsNeverRead()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
    f.CarryTheEntry(true);
    f.startup.service.Manage(false);

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    QVERIFY(f.service.StartupEntriesCarriedBy(profile, snapshot, f.Pmdg(snapshot)).empty());
    QCOMPARE(f.startup.entries.reads, std::size_t{0});
}

void ProfileServiceTest::ADisabledEntryInsideTheAddonIsNotWorthAsking()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
    f.CarryTheEntry(false);

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    QVERIFY(f.service.StartupEntriesCarriedBy(profile, snapshot, f.Pmdg(snapshot)).empty());
}

void ProfileServiceTest::AnEntryInsideADestinationButOutsideTheAddonIsNotWorthAsking()
{
    const std::filesystem::path elsewhere = std::filesystem::path(kCommunity) / "aerosoft-crj" / "bin" / "updater.exe";

    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
    f.fileSystem.AddFile(elsewhere);
    f.startup.entries.Carry(StartupEntry{.label = "CRJ Updater", .path = elsewhere, .enabled = true});

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    QVERIFY(f.service.StartupEntriesCarriedBy(profile, snapshot, f.Pmdg(snapshot)).empty());
}

void ProfileServiceTest::TheStartupStepsOfABatchShareOneBackupRecord()
{
    Fixture f;
    f.CarryThreeStrangers();

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, Fixture::TurningOffTheThreeStrangers()).results;

    QCOMPARE(results.size(), std::size_t{3});
    QCOMPARE(f.startup.entries.switchesThatFoundNoBackup, std::size_t{1});
    QCOMPARE(f.startup.entries.switchesThatFoundTheBackupTaken, std::size_t{2});
    QCOMPARE(f.journal.appended.size(), std::size_t{3});
}

void ProfileServiceTest::UndoKeepsABackupRecordOfItsOwn()
{
    Fixture f;
    f.CarryThreeStrangers();

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    static_cast<void>(f.service.SetEnabled(profile, snapshot, Fixture::TurningOffTheThreeStrangers()));

    const std::vector<LinkOperationResult> reverted = f.service.UndoLastBatch();

    QCOMPARE(reverted.size(), std::size_t{3});
    QCOMPARE(f.startup.entries.switchesThatFoundNoBackup, std::size_t{2});
    QCOMPARE(f.startup.entries.switchesThatFoundTheBackupTaken, std::size_t{4});
}

namespace
{
    void PutTheLinkOnTheOtherProgramsFolder(Fixture& f)
    {
        f.fileSystem.AddDirectory("C:/Program Files (x86)/Addon Manager/MSFS");
        f.fileSystem.AddDirectory(kVendorFolder);
        f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", kVendorFolder);
        f.fileSystem.AddFileWithContents(ExternalSidecarPathFor(kImportedExternal),
                                         TextOfTheExternalOrigin(kVendorFolder));
    }
}

void ProfileServiceTest::ALinkToTheOtherProgramsFolderWhoseCopyIsKnownOnlyBySidecarIsReadAsDivergent()
{
    Fixture f;
    PutTheLinkOnTheOtherProgramsFolder(f);

    const SimulatorProfile forgetful = Profile();
    QVERIFY(forgetful.externalOrigins.empty());

    const ProfileSnapshot shown = f.Snapshot(forgetful);
    QCOMPARE(shown.entries.size(), std::size_t{1});
    QCOMPARE(shown.entries.front().classification, EntryClassification::Divergent);

    const ProfileService::LinksOnDisk onDisk = f.service.ReadLinksNow(forgetful, shown.libraries);

    QCOMPARE(onDisk.entries.size(), std::size_t{1});
    QCOMPARE(onDisk.entries.front().classification, EntryClassification::Divergent);
    QVERIFY(onDisk.enabled.Contains(kVendorFolder));
}

void ProfileServiceTest::ThePlaceTakenByThatLinkIsSeenByThePlacesTakenReadThroughTheLibraries()
{
    Fixture f;
    const SimulatorProfile profile = ProfileWithASecondLibraryHolding(f, "F:/Extra/pmdg-aircraft-77w");
    PutTheLinkOnTheOtherProgramsFolder(f);

    const ProfileSnapshot snapshot = f.Snapshot(profile);
    const std::vector<const TreeNode*> wanted{&snapshot.libraries[1].children.front()};

    const std::vector<TakenPlace> throughTheLibraries = f.service.PlacesTaken(profile, wanted, snapshot.libraries);

    QCOMPARE(throughTheLibraries.size(), std::size_t{1});
    QCOMPARE(throughTheLibraries.front().occupant, std::filesystem::path{kVendorFolder});
}

void ProfileServiceTest::ThePlaceTakenByThatLinkIsSeenByThePlacesTakenNowRead()
{
    Fixture f;
    const SimulatorProfile profile = ProfileWithASecondLibraryHolding(f, "F:/Extra/pmdg-aircraft-77w");
    PutTheLinkOnTheOtherProgramsFolder(f);

    const ProfileSnapshot snapshot = f.Snapshot(profile);
    const std::vector<const TreeNode*> wanted{&snapshot.libraries[1].children.front()};

    const std::vector<TakenPlace> throughTheSnapshot = f.service.PlacesTakenNow(profile, wanted, snapshot);

    QCOMPARE(throughTheSnapshot.size(), std::size_t{1});
    QCOMPARE(throughTheSnapshot.front().occupant, std::filesystem::path{kVendorFolder});
}

namespace
{
    class CountingSidecarReads final : public SidecarStore
    {
    public:
        explicit CountingSidecarReads(SidecarStore& inner) : inner_(inner)
        {
        }

        [[nodiscard]] bool Write(const std::filesystem::path& path, const std::string& contents) override
        {
            return inner_.Write(path, contents);
        }

        [[nodiscard]] std::optional<std::string> Read(const std::filesystem::path& path) const override
        {
            ++reads;

            return inner_.Read(path);
        }

        [[nodiscard]] bool Forget(const std::filesystem::path& path) override
        {
            return inner_.Forget(path);
        }

        mutable std::size_t reads = 0;

    private:
        SidecarStore& inner_;
    };
}

void ProfileServiceTest::PlanningOverPlacesThatAreFreeReadsNoSidecar()
{
    Fixture f;
    const SimulatorProfile profile = Profile();
    const ProfileSnapshot shown = f.Snapshot(profile);
    CountingSidecarReads counting{f.sidecars};
    ProfileService service{f.catalog, f.filesystemProbe, counting,          f.classifier,      f.linking,
                           f.log,     f.identities,      f.startup.service, LinkType::Junction};

    QVERIFY(service.PlacesTakenNow(profile, {Fixture::AddonAt(shown, 0)}, shown).empty());
    QCOMPARE(counting.reads, std::size_t{0});

    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", "D:/MSFS 2024/Aircrafts/aerosoft-crj");

    QCOMPARE(service.PlacesTakenNow(profile, {Fixture::AddonAt(shown, 0)}, shown).size(), std::size_t{1});
    QVERIFY(counting.reads > 0);
}

void ProfileServiceTest::PlanningReadsEachPlaceOnceEvenWhenSomeAreTaken()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", "D:/MSFS 2024/Aircrafts/aerosoft-crj");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot shown = f.Snapshot(profile);
    const std::vector<const TreeNode*> wanted = Fixture::Aircrafts(shown);
    f.linkService.ForgetTheReads();

    QCOMPARE(f.service.PlacesTakenNow(profile, wanted, shown).size(), std::size_t{1});

    QCOMPARE(f.linkService.SingleReads(), std::size_t{0});
    QCOMPARE(f.linkService.BatchReads(), std::size_t{1});
    QCOMPARE(f.linkService.PlacesRead(), std::size_t{2});
    QCOMPARE(f.linkService.TimesRead("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"), std::size_t{1});
    QCOMPARE(f.linkService.TimesRead("E:/Flight Simulator 2024/Community/fenix-a320"), std::size_t{1});
}

void ProfileServiceTest::ThePlaceTakenIsTheEffectiveDestinationWithAndWithoutAnOverride()
{
    const SimulatorProfile profile = Profile(
        {DestinationOverride{
             .libraryId = kLibraryId, .relativePath = "Aircrafts/aerosoft-crj", .destination = kCommunity2024},
         DestinationOverride{
             .libraryId = kLibraryId, .relativePath = "Aircrafts/fenix-a320", .destination = "Z:/Not/A/Destination"}});

    Fixture f;
    f.fileSystem.AddDirectory("D:/MSFS 2024/Elsewhere/occupant");

    const std::vector<std::filesystem::path> addons{"D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w",
                                                    "D:/MSFS 2024/Aircrafts/aerosoft-crj",
                                                    "D:/MSFS 2024/Aircrafts/fenix-a320"};
    for (const std::filesystem::path& addon : addons)
    {
        f.fileSystem.AddLink(PlannedLinkPath(profile, addon), "D:/MSFS 2024/Elsewhere/occupant");
    }

    const ProfileSnapshot shown = f.Snapshot(profile);

    const std::vector<TakenPlace> taken = f.service.PlacesTakenNow(profile, Fixture::Aircrafts(shown), shown);

    QCOMPARE(taken.size(), addons.size());
    QCOMPARE(taken[1].linkPath, std::filesystem::path{"E:/Flight Simulator 2024/Community2024/aerosoft-crj"});

    for (std::size_t index = 0; index < addons.size(); ++index)
    {
        QCOMPARE(taken[index].addonFolder, addons[index]);
        QCOMPARE(taken[index].linkPath, PlannedLinkPath(profile, addons[index]));
        QCOMPARE(taken[index].occupant, std::filesystem::path{"D:/MSFS 2024/Elsewhere/occupant"});
    }
}

void ProfileServiceTest::EnablingReadsTheDestinationsOnceAndDerivesTheEntriesAfterwards()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/aerosoft-crj", "D:/MSFS 2024/Aircrafts/aerosoft-crj");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot shown = f.Snapshot(profile);
    f.linkService.ForgetTheReads();

    const LinkBatchOutcome outcome = f.service.SetEnabled(
        EntriesStamp{.profile = profile}, shown, LinkBatch{.toDisable = {}, .toEnable = {Fixture::AddonAt(shown, 0)}});

    QCOMPARE(outcome.report.results.size(), std::size_t{1});
    QCOMPARE(f.linkService.TimesRead("E:/Flight Simulator 2024/Community/aerosoft-crj"), std::size_t{1});
    QCOMPARE(f.linkService.TimesRead("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"), std::size_t{1});
    QCOMPARE(f.linkService.PlacesRead(), std::size_t{2});
    QCOMPARE(outcome.read.entries.size(), std::size_t{2});
    QVERIFY(outcome.read.enabled.Contains("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"));
    QVERIFY(outcome.read.enabled.Contains("D:/MSFS 2024/Aircrafts/aerosoft-crj"));
}

void ProfileServiceTest::RelinkingReadsTheDestinationsOnceAndDerivesTheEntriesAfterwards()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot shown = f.Snapshot(profile);
    f.linkService.ForgetTheReads();

    const LinkBatchOutcome outcome =
        f.service.Relink(EntriesStamp{.profile = profile}, shown, {Fixture::AddonAt(shown, 0)});

    QCOMPARE(outcome.report.results.size(), std::size_t{2});
    QCOMPARE(f.linkService.TimesRead("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w"), std::size_t{1});
    QCOMPARE(f.linkService.TimesRead("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"), std::size_t{1});
    QCOMPARE(f.linkService.PlacesRead(), std::size_t{2});
    QCOMPARE(outcome.read.entries.size(), std::size_t{1});
    QCOMPARE(outcome.read.entries.front().path,
             std::filesystem::path{"E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"});
    QVERIFY(outcome.read.enabled.Contains("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"));
}

void ProfileServiceTest::UndoingReadsTheDestinationsOnceAndDerivesTheEntriesAfterwards()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/aerosoft-crj", "D:/MSFS 2024/Aircrafts/aerosoft-crj");

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot shown = f.Snapshot(profile);

    const LinkBatchOutcome enabled = f.service.SetEnabled(
        EntriesStamp{.profile = profile}, shown, LinkBatch{.toDisable = {}, .toEnable = {Fixture::AddonAt(shown, 0)}});
    QCOMPARE(enabled.read.entries.size(), std::size_t{2});

    f.linkService.ForgetTheReads();

    const LinkBatchOutcome undone = f.service.UndoLastBatch(EntriesStamp{.profile = profile}, shown.libraries);

    QCOMPARE(undone.report.results.size(), std::size_t{1});
    QCOMPARE(f.linkService.TimesRead("E:/Flight Simulator 2024/Community/aerosoft-crj"), std::size_t{1});
    QCOMPARE(f.linkService.TimesRead("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"), std::size_t{1});
    QCOMPARE(f.linkService.PlacesRead(), std::size_t{2});
    QCOMPARE(undone.read.entries.size(), std::size_t{1});
    QVERIFY(undone.read.enabled.Contains("D:/MSFS 2024/Aircrafts/aerosoft-crj"));
    QVERIFY(!undone.read.enabled.Contains("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"));
}

void ProfileServiceTest::TheOutcomeOfABatchCarriesTheStampItWasAskedFor()
{
    Fixture f;
    f.CarryThreeStrangers();

    const SimulatorProfile profile = Profile();
    const ProfileSnapshot shown = f.Snapshot(profile);

    const LinkBatchOutcome outcome = f.service.SetEnabled(EntriesStamp{.profile = profile, .adoptions = 7}, shown,
                                                          Fixture::TurningOffTheThreeStrangers());

    QCOMPARE(outcome.read.stamp.adoptions, 7);
    QCOMPARE(outcome.read.stamp.profile.id, profile.id);
    QCOMPARE(outcome.read.startupEntries.size(), std::size_t{3});
    QVERIFY(!outcome.read.startupEntries.front().enabled);
}

QTEST_APPLESS_MAIN(ProfileServiceTest)

#include "tst_profile_service.moc"
