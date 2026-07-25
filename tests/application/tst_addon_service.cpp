#include <QtTest/QtTest>

#include "application/AddonService.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

class AddonServiceTest : public QObject
{
    Q_OBJECT

private slots:
    static void MarkingACategoryEnablesEveryAddonUnderIt();
    static void ABatchKeepsGoingAfterAFailureAndReportsOneResultPerItem();
    static void AFailedItemInABatchDoesNotUndoTheItemsThatWorked();
    static void AlreadyEnabledAddonsAreLeftAloneInsteadOfReportedAsOccupied();
    static void DisablingAnAddonRemovesItsLinkInEveryDestination();
    static void EnablingHonoursTheDestinationOverrideOfTheCategory();
    static void EveryLinkOperationReachesTheJournalWhetherItWorkedOrNot();
    static void UndoRevertsTheLastBatchAndNothingElse();
    static void UndoOnlyRevertsWhatTheBatchActuallyDid();
    static void ThereIsNothingToUndoBeforeTheFirstBatch();
    static void RegisteringALibraryReportsWhatIsInsideAndRefusesANestedFolder();
    static void ABatchWithNothingToDoDoesNotThrowAwayThePreviousUndo();
    static void ABatchWhereEveryStepFailedKeepsThePreviousUndo();
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
        TreeNode node = CategoryNode(kLibrary, {
                                         CategoryNode("D:/MSFS 2024/Aircrafts", {
                                                          AddonNode("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"),
                                                          AddonNode("D:/MSFS 2024/Aircrafts/aerosoft-crj"),
                                                          AddonNode("D:/MSFS 2024/Aircrafts/fenix-a320")
                                                      })
                                     });
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

        [[nodiscard]] TreeSnapshot Snapshot(const SimulatorProfile& profile) const
        {
            return service.Scan(profile);
        }

        [[nodiscard]] static std::vector<const TreeNode*> Aircrafts(const TreeSnapshot& snapshot)
        {
            return {&snapshot.libraries.front().children.front()};
        }

        [[nodiscard]] static const TreeNode* AddonAt(const TreeSnapshot& snapshot, const std::size_t index)
        {
            return &snapshot.libraries.front().children.front().children[index];
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFileOperations fileOperations{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, fileOperations};
        EnabledStateResolver resolver{linkService, fileOperations};
        AddonService service{catalog, resolver, linking, journal, clock, identities, LinkType::Junction};
    };
}

void AddonServiceTest::MarkingACategoryEnablesEveryAddonUnderIt()
{
    Fixture f;
    const SimulatorProfile profile = Profile();
    const TreeSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true);

    QCOMPARE(results.size(), std::size_t{3});
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/aerosoft-crj"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/fenix-a320"));
}

void AddonServiceTest::ABatchKeepsGoingAfterAFailureAndReportsOneResultPerItem()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const TreeSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true);

    QCOMPARE(results.size(), std::size_t{3});
    QCOMPARE(results.front().outcome.Failure(), LinkFailure::DestinationHoldsRealFolder);
    QVERIFY(results[1].outcome.Succeeded());
    QVERIFY(results[2].outcome.Succeeded());
}

void AddonServiceTest::AFailedItemInABatchDoesNotUndoTheItemsThatWorked()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const TreeSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true);

    QCOMPARE(results.size(), std::size_t{3});
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/aerosoft-crj"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/fenix-a320"));
    QVERIFY(f.fileSystem.IsDirectory("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
}

void AddonServiceTest::AlreadyEnabledAddonsAreLeftAloneInsteadOfReportedAsOccupied()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const TreeSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true);

    QCOMPARE(results.size(), std::size_t{2});
    for (const LinkOperationResult& result : results)
    {
        QVERIFY(result.outcome.Succeeded());
    }
}

void AddonServiceTest::DisablingAnAddonRemovesItsLinkInEveryDestination()
{
    const std::filesystem::path folder = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";

    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", folder);
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w", folder);

    const SimulatorProfile profile = Profile();
    const TreeSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, {Fixture::AddonAt(snapshot, 0)}, false);

    QCOMPARE(results.size(), std::size_t{2});
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsDirectory(folder));
}

void AddonServiceTest::EnablingHonoursTheDestinationOverrideOfTheCategory()
{
    Fixture f;
    const SimulatorProfile profile = Profile({{kLibraryId, "Aircrafts", kCommunity2024}});
    const TreeSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, {Fixture::AddonAt(snapshot, 0)}, true);

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().linkPath,
             std::filesystem::path("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w"));
}

void AddonServiceTest::EveryLinkOperationReachesTheJournalWhetherItWorkedOrNot()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const TreeSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true);

    QCOMPARE(results.size(), std::size_t{3});
    QCOMPARE(f.journal.appended.size(), std::size_t{3});

    const OperationRecord& failed = f.journal.appended.front();
    QCOMPARE(failed.kind, OperationKind::EnableAddon);
    QCOMPARE(failed.failure, LinkFailure::DestinationHoldsRealFolder);
    QCOMPARE(failed.addonId, (AddonId{kLibraryId, "pmdg-aircraft-77w"}));
    QCOMPARE(failed.source, std::filesystem::path("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"));
    QCOMPARE(failed.target, std::filesystem::path("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QCOMPARE(failed.timestamp, f.clock.now);

    QCOMPARE(f.journal.appended[1].failure, LinkFailure::None);
}

void AddonServiceTest::UndoRevertsTheLastBatchAndNothingElse()
{
    Fixture f;
    const SimulatorProfile profile = Profile();

    const TreeSnapshot first = f.Snapshot(profile);
    const std::vector<LinkOperationResult> kept =
        f.service.SetEnabled(profile, first, {Fixture::AddonAt(first, 0), Fixture::AddonAt(first, 1)}, true);

    const TreeSnapshot second = f.Snapshot(profile);
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

void AddonServiceTest::UndoOnlyRevertsWhatTheBatchActuallyDid()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const TreeSnapshot snapshot = f.Snapshot(profile);

    const std::vector<LinkOperationResult> results =
        f.service.SetEnabled(profile, snapshot, Fixture::Aircrafts(snapshot), true);
    const std::vector<LinkOperationResult> reverted = f.service.UndoLastBatch();

    QCOMPARE(results.size(), std::size_t{3});
    QCOMPARE(reverted.size(), std::size_t{2});
    QVERIFY(f.fileSystem.IsDirectory("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/aerosoft-crj"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/fenix-a320"));
}

void AddonServiceTest::ThereIsNothingToUndoBeforeTheFirstBatch()
{
    const Fixture f;

    QVERIFY(!f.service.CanUndo());
}

void AddonServiceTest::RegisteringALibraryReportsWhatIsInsideAndRefusesANestedFolder()
{
    Fixture f;
    SimulatorProfile profile = Profile();
    profile.libraries.clear();

    const LibraryReport accepted = f.service.RegisterLibrary(profile, kLibrary);

    QVERIFY(accepted.accepted);
    QCOMPARE(accepted.categories, std::size_t{1});
    QCOMPARE(accepted.addons, std::size_t{3});
    QCOMPARE(profile.libraries.size(), std::size_t{1});

    const LibraryReport nested = f.service.RegisterLibrary(profile, "D:/MSFS 2024/Aircrafts");

    QVERIFY(!nested.accepted);
    QCOMPARE(nested.addons, std::size_t{0});
    QCOMPARE(profile.libraries.size(), std::size_t{1});
}

void AddonServiceTest::ABatchWithNothingToDoDoesNotThrowAwayThePreviousUndo()
{
    Fixture f;
    const SimulatorProfile profile = Profile();

    const TreeSnapshot before = f.Snapshot(profile);
    const std::vector<LinkOperationResult> done =
        f.service.SetEnabled(profile, before, {Fixture::AddonAt(before, 0)}, true);

    const TreeSnapshot after = f.Snapshot(profile);
    const std::vector<LinkOperationResult> again =
        f.service.SetEnabled(profile, after, {Fixture::AddonAt(after, 0)}, true);

    QCOMPARE(done.size(), std::size_t{1});
    QVERIFY(again.empty());
    QVERIFY(f.service.CanUndo());

    QCOMPARE(f.service.UndoLastBatch().size(), std::size_t{1});
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
}

void AddonServiceTest::ABatchWhereEveryStepFailedKeepsThePreviousUndo()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/aerosoft-crj");

    const SimulatorProfile profile = Profile();

    const TreeSnapshot before = f.Snapshot(profile);
    const std::vector<LinkOperationResult> done =
        f.service.SetEnabled(profile, before, {Fixture::AddonAt(before, 0)}, true);

    const TreeSnapshot after = f.Snapshot(profile);
    const std::vector<LinkOperationResult> failed =
        f.service.SetEnabled(profile, after, {Fixture::AddonAt(after, 1)}, true);

    QCOMPARE(done.size(), std::size_t{1});
    QCOMPARE(failed.size(), std::size_t{1});
    QCOMPARE(failed.front().outcome.Failure(), LinkFailure::DestinationHoldsRealFolder);
    QVERIFY(f.service.CanUndo());

    QCOMPARE(f.service.UndoLastBatch().size(), std::size_t{1});
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
}

QTEST_APPLESS_MAIN(AddonServiceTest)

#include "tst_addon_service.moc"
