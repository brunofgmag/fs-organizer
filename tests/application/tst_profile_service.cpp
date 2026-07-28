#include <QtTest/QtTest>

#include <variant>

#include "domain/journal/OperationLog.h"
#include "application/ProfileService.h"
#include "domain/linking/RepairPlan.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

class ProfileServiceTest : public QObject
{
    Q_OBJECT

private slots:
    static void MarkingACategoryEnablesEveryAddonUnderIt();
    static void ABatchKeepsGoingAfterAFailureAndReportsOneResultPerItem();
    static void AFailedItemInABatchDoesNotUndoTheItemsThatWorked();
    static void AlreadyEnabledAddonsAreLeftAloneInsteadOfReportedAsOccupied();
    static void DisablingAnAddonRemovesItsLinkInEveryDestination();
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
    static void ForgettingTheUndoLeavesTheLinksInPlaceAndOnlyDropsTheBatch();
    static void ABatchThatTurnsSomeOffAndOthersOnUndoesAsOnePiece();
    static void TheDisablesRunBeforeTheEnablesSoTheDestinationIsFree();
    static void UndoingASwapPutsTheOldAddonBackInTheDestination();
};

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kCommunity2024 = "E:/Flight Simulator 2024/Community2024";
    constexpr auto kLibraryId = "library-1";

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
        profile.libraries = {Library{kLibraryId, kLibrary, "MSFS 2024"}};
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

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        ProfileService service{catalog, classifier, linking, log, identities, LinkType::Junction};
    };
}

void ProfileServiceTest::MarkingACategoryEnablesEveryAddonUnderIt()
{
    Fixture f;
    const SimulatorProfile profile = Profile();
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true);

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
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true);

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
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true);

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
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true);

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
        f.service.SetEnabled(profile, snapshot, {Fixture::AddonAt(snapshot, 0)}, false);

    QCOMPARE(results.size(), std::size_t{2});
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsDirectory(folder));
}

void ProfileServiceTest::EnablingHonoursTheDestinationOverrideOfTheCategory()
{
    Fixture f;
    const SimulatorProfile profile = Profile({{kLibraryId, "Aircrafts", kCommunity2024}});
    const ProfileSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, {Fixture::AddonAt(snapshot, 0)}, true);

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().linkPath,
             std::filesystem::path("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w"));
}

void ProfileServiceTest::TurningAnAddonOffAndOnAgainLeavesItInTheDestinationItLivedIn()
{
    Fixture f;
    const SimulatorProfile profile = Profile({{kLibraryId, "Aircrafts", kCommunity2024}});

    const ProfileSnapshot enabled = f.Snapshot(profile);
    QCOMPARE(f.service.SetEnabled(profile, enabled, {Fixture::AddonAt(enabled, 1)}, true).size(), std::size_t{1});
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community2024/aerosoft-crj"));

    const ProfileSnapshot afterEnabling = f.Snapshot(profile);
    QCOMPARE(f.service.SetEnabled(profile, afterEnabling, {Fixture::AddonAt(afterEnabling, 1)}, false).size(),
             std::size_t{1});
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community2024/aerosoft-crj"));

    const ProfileSnapshot afterDisabling = f.Snapshot(profile);
    const std::vector<LinkOperationResult> again =
        f.service.SetEnabled(profile, afterDisabling, {Fixture::AddonAt(afterDisabling, 1)}, true);

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
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true);

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
        f.service.SetEnabled(profile, first, {Fixture::AddonAt(first, 0), Fixture::AddonAt(first, 1)}, true);

    const ProfileSnapshot second = f.Snapshot(profile);
    const std::vector<LinkOperationResult> undone =
        f.service.SetEnabled(profile, second, {Fixture::AddonAt(second, 2)}, true);

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
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true);
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
        f.service.SetEnabled(profile, before, {Fixture::AddonAt(before, 0)}, true);

    const ProfileSnapshot after = f.Snapshot(profile);
    const std::vector<LinkOperationResult> again =
        f.service.SetEnabled(profile, after, {Fixture::AddonAt(after, 0)}, true);

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
        f.service.SetEnabled(profile, before, {Fixture::AddonAt(before, 0)}, true);

    const ProfileSnapshot after = f.Snapshot(profile);
    const std::vector<LinkOperationResult> failed =
        f.service.SetEnabled(profile, after, {Fixture::AddonAt(after, 1)}, true);

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
            requests.push_back({candidate, action});
        }

        return requests;
    }
}

void ProfileServiceTest::RepairingRemovesTheDeadNodeAndJournalsIt()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/gone", "D:/Removed Library/gone");

    const SimulatorProfile profile = Profile();
    const std::vector<LinkOperationResult> results =
        f.service.Repair(profile, Requests(f, profile, RepairAction::RemoveDeadNode));

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
        f.service.Repair(profile, Requests(f, profile, RepairAction::Repoint));

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
        f.service.Repair(profile, Requests(f, profile, RepairAction::RemoveDeadNode));

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
        f.service.Repair(profile, Requests(f, profile, RepairAction::Repoint));

    QCOMPARE(repaired.size(), std::size_t{1});

    const std::vector<LinkOperationResult> reverted = f.service.UndoLastBatch();

    QCOMPARE(reverted.size(), std::size_t{2});
    QCOMPARE(f.fileSystem.LinkTarget("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"),
             std::optional<std::filesystem::path>("D:/Old Library/pmdg-aircraft-77w"));
}

void ProfileServiceTest::ForgettingTheUndoLeavesTheLinksInPlaceAndOnlyDropsTheBatch()
{
    Fixture f;
    const SimulatorProfile profile = Profile();

    const ProfileSnapshot snapshot = f.Snapshot(profile);
    QCOMPARE(f.service.SetEnabled(profile, snapshot, {Fixture::AddonAt(snapshot, 0)}, true).size(), std::size_t{1});
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

    const std::vector<LinkOperationResult> results = f.service.SetEnabled(
        profile, snapshot, LinkBatch{{Fixture::AddonAt(snapshot, 0)}, {Fixture::AddonAt(snapshot, 1)}});

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
    profile.libraries.push_back(Library{"library-2", "F:/Extra", "Extra"});

    const ProfileSnapshot snapshot = f.Snapshot(profile);
    const TreeNode* replacement = &snapshot.libraries[1].children.front();

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, LinkBatch{{Fixture::AddonAt(snapshot, 0)}, {replacement}});

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
    profile.libraries.push_back(Library{"library-2", "F:/Extra", "Extra"});

    const ProfileSnapshot snapshot = f.Snapshot(profile);
    const TreeNode* replacement = &snapshot.libraries[1].children.front();

    QCOMPARE(f.service.SetEnabled(profile, snapshot, LinkBatch{{Fixture::AddonAt(snapshot, 0)}, {replacement}}).size(),
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

QTEST_APPLESS_MAIN(ProfileServiceTest)

#include "tst_profile_service.moc"
