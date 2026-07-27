#include <QtTest/QtTest>

#include "application/LibraryOrganizer.h"
#include "application/Session.h"
#include "domain/journal/OperationLog.h"
#include "domain/support/PathUtils.h"
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
#include "tests/doubles/RecordingSessionObserver.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

class SessionTest : public QObject
{
    Q_OBJECT

private slots:
    static void TheProfileAndItsSnapshotOnlyLandTogetherWhenTheScanFinishes();
    static void AScanAskedForWhileAnotherIsRunningIsRunAgainInsteadOfLost();
    static void ChoosingAProfileRemembersTheChoiceBeforeReadingTheDisk();
    static void RegisteringALibrarySavesTheProfileAndReadsTheDiskAgain();
    static void OverridingADestinationSavesTheProfileWithoutAnotherScan();
    static void OverridingADestinationForAWholeSelectionSavesOnceInsteadOfOncePerNode();
    static void ADestinationAskedForOutsideEveryLibraryChangesNothingAndSavesNothing();
    static void RefreshingTheEntriesSeesALinkThatAppearedAfterTheScan();
    static void RefreshingTheEntriesSeesACopyThatAppearedAfterTheScan();
    static void CreatingACategoryReadsTheDiskAgainSoTheTreeShowsIt();
    static void RenamingACategorySavesTheCarriedOverridesAndReadsTheDiskAgain();
    static void ARefusedCategoryLeavesTheProfileAndTheDiskAlone();
    static void MovingAnAddonCarriesItsOverrideAndReadsTheDiskAgain();
};

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kExtraLibrary = "D:/MSFS 2024 Extra";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kOtherDestination = "E:/Flight Simulator 2024/Community2024";
    constexpr auto kAircrafts = "D:/MSFS 2024/Aircrafts";
    constexpr auto kSceneries = "D:/MSFS 2024/Sceneries";
    constexpr auto kAddon = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";

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
        TreeNode node = CategoryNode(kLibrary, {CategoryNode("D:/MSFS 2024/Aircrafts", {AddonNode(kAddon)})});
        node.kind = TreeNodeKind::Library;

        return node;
    }

    SimulatorProfile Profile(const std::string& id)
    {
        SimulatorProfile profile;
        profile.id = id;
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity, kOtherDestination};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{"library-1", kLibrary, "MSFS 2024"}};

        return profile;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory(kOtherDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kAddon);
            catalog.SetTree(kLibrary, LibraryTree());

            settings.stored.profiles = {Profile("msfs2024")};
            settings.stored.activeProfileId = "msfs2024";
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
        RecordingSessionObserver observer;
        Session session{service, organizer, settings, runner, observer};
    };
}

void SessionTest::TheProfileAndItsSnapshotOnlyLandTogetherWhenTheScanFinishes()
{
    Fixture f;
    f.runner.defer = true;

    f.session.ShowActiveProfile();

    QVERIFY(f.session.Scanning());
    QVERIFY(f.session.Profile().id.empty());
    QVERIFY(f.session.Snapshot().libraries.empty());
    QCOMPARE(f.observer.started, 1);
    QCOMPARE(f.observer.finished, 0);

    f.runner.Finish();

    QVERIFY(!f.session.Scanning());
    QCOMPARE(QString::fromStdString(f.session.Profile().id), QStringLiteral("msfs2024"));
    QCOMPARE(f.session.Snapshot().libraries.size(), std::size_t{1});
    QCOMPARE(f.observer.finished, 1);
}

void SessionTest::AScanAskedForWhileAnotherIsRunningIsRunAgainInsteadOfLost()
{
    Fixture f;
    f.runner.defer = true;

    f.session.ShowActiveProfile();
    f.session.ShowActiveProfile();

    QCOMPARE(f.runner.runs, 1);
    QCOMPARE(f.observer.started, 1);

    f.runner.Finish();

    QCOMPARE(f.runner.runs, 2);
    QCOMPARE(f.observer.started, 2);
    QCOMPARE(f.observer.finished, 0);
    QVERIFY(f.session.Scanning());

    f.runner.Finish();

    QCOMPARE(f.observer.finished, 1);
    QVERIFY(!f.session.Scanning());
    QCOMPARE(f.session.Snapshot().libraries.size(), std::size_t{1});
}

void SessionTest::ChoosingAProfileRemembersTheChoiceBeforeReadingTheDisk()
{
    Fixture f;
    f.settings.stored.profiles.push_back(Profile("msfs2020"));

    f.session.ChooseProfile("msfs2020");

    QCOMPARE(QString::fromStdString(f.settings.stored.activeProfileId), QStringLiteral("msfs2020"));
    QCOMPARE(QString::fromStdString(f.session.Profile().id), QStringLiteral("msfs2020"));
    QCOMPARE(f.observer.finished, 1);
}

void SessionTest::RegisteringALibrarySavesTheProfileAndReadsTheDiskAgain()
{
    Fixture f;
    f.session.ShowActiveProfile();
    f.fileSystem.AddDirectory(kExtraLibrary);
    f.catalog.SetTree(kExtraLibrary, TreeNode{});

    const LibraryReport report = f.session.RegisterLibrary(kExtraLibrary);

    QVERIFY(report.Accepted());
    QCOMPARE(f.session.Profile().libraries.size(), std::size_t{2});
    QCOMPARE(f.settings.stored.profiles.front().libraries.size(), std::size_t{2});
    QCOMPARE(f.observer.finished, 2);
}

void SessionTest::OverridingADestinationSavesTheProfileWithoutAnotherScan()
{
    Fixture f;
    f.session.ShowActiveProfile();

    const TreeNode addon = AddonNode(kAddon);

    f.session.OverrideDestination({&addon}, kOtherDestination);

    QCOMPARE(f.session.Profile().destinationOverrides.size(), std::size_t{1});
    QCOMPARE(f.settings.stored.profiles.front().destinationOverrides.size(), std::size_t{1});
    QCOMPARE(f.observer.refreshed, 1);
    QCOMPARE(f.observer.finished, 1);
}

void SessionTest::OverridingADestinationForAWholeSelectionSavesOnceInsteadOfOncePerNode()
{
    Fixture f;
    f.session.ShowActiveProfile();

    const TreeNode addon = AddonNode(kAddon);
    const TreeNode category = AddonNode(kSceneries);

    f.session.OverrideDestination({&addon, &category}, kOtherDestination);

    QCOMPARE(f.session.Profile().destinationOverrides.size(), std::size_t{2});
    QCOMPARE(f.settings.stored.profiles.front().destinationOverrides.size(), std::size_t{2});
    QCOMPARE(f.observer.refreshed, 1);
}

void SessionTest::ADestinationAskedForOutsideEveryLibraryChangesNothingAndSavesNothing()
{
    Fixture f;
    f.session.ShowActiveProfile();

    const TreeNode stranger = AddonNode("E:/Somewhere Else/addon");

    f.session.OverrideDestination({&stranger}, kOtherDestination);

    QVERIFY(f.session.Profile().destinationOverrides.empty());
    QCOMPARE(f.observer.refreshed, 0);
}

void SessionTest::RefreshingTheEntriesSeesALinkThatAppearedAfterTheScan()
{
    Fixture f;
    f.session.ShowActiveProfile();

    QVERIFY(!f.session.Snapshot().enabled.Contains(kAddon));

    f.fileSystem.AddLink(std::filesystem::path(kCommunity) / "pmdg-aircraft-77w", kAddon);
    f.session.RefreshEntries();

    QVERIFY(f.session.Snapshot().enabled.Contains(kAddon));
    QCOMPARE(f.observer.refreshed, 1);
    QCOMPARE(f.observer.finished, 1);
}

void SessionTest::RefreshingTheEntriesSeesACopyThatAppearedAfterTheScan()
{
    Fixture f;
    f.session.ShowActiveProfile();

    QCOMPARE(f.session.Snapshot().conflicts.Count(), std::size_t{0});

    f.fileSystem.AddDirectory(std::filesystem::path(kCommunity) / "pmdg-aircraft-77w");
    f.session.RefreshEntries();

    QCOMPARE(f.session.Snapshot().conflicts.Count(), std::size_t{1});
}

void SessionTest::CreatingACategoryReadsTheDiskAgainSoTheTreeShowsIt()
{
    Fixture f;
    f.session.ShowActiveProfile();

    const FileOperationResult result = f.session.CreateCategory(kLibrary, "Sceneries");

    QCOMPARE(result.result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists(std::filesystem::path(kLibrary) / "Sceneries"));
    QCOMPARE(f.observer.started, 2);
    QCOMPARE(f.observer.finished, 2);
}

void SessionTest::RenamingACategorySavesTheCarriedOverridesAndReadsTheDiskAgain()
{
    Fixture f;
    f.fileSystem.AddDirectory(kAircrafts);
    f.settings.stored.profiles.front().destinationOverrides = {
        DestinationOverride{"library-1", "Aircrafts", kOtherDestination}};
    f.session.ShowActiveProfile();

    const FileOperationResult result = f.session.RenameCategory(kAircrafts, "Airplanes");

    QCOMPARE(result.result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists("D:/MSFS 2024/Airplanes"));
    QCOMPARE(ComparablePath(f.settings.stored.profiles.front().destinationOverrides.front().relativePath),
             ComparablePath("Airplanes"));
    QCOMPARE(f.observer.started, 2);
}

void SessionTest::ARefusedCategoryLeavesTheProfileAndTheDiskAlone()
{
    Fixture f;
    f.fileSystem.AddDirectory(kAircrafts);
    f.settings.stored.profiles.front().destinationOverrides = {
        DestinationOverride{"library-1", "Aircrafts", kOtherDestination}};
    f.session.ShowActiveProfile();
    f.processProbe.ReportTheSimulatorAsRunning();

    QCOMPARE(f.session.RenameCategory(kAircrafts, "Airplanes").result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(f.session.CreateCategory(kLibrary, "Sceneries").result, FileResult::TheSimulatorIsRunning);

    QVERIFY(!f.fileSystem.Exists("D:/MSFS 2024/Airplanes"));
    QVERIFY(!f.fileSystem.Exists("D:/MSFS 2024/Sceneries"));
    QCOMPARE(ComparablePath(f.settings.stored.profiles.front().destinationOverrides.front().relativePath),
             ComparablePath("Aircrafts"));
    QCOMPARE(f.observer.started, 1);
}

void SessionTest::MovingAnAddonCarriesItsOverrideAndReadsTheDiskAgain()
{
    Fixture f;
    f.fileSystem.AddDirectory(kAircrafts);
    f.fileSystem.AddDirectory(kSceneries);
    f.settings.stored.profiles.front().destinationOverrides = {
        DestinationOverride{"library-1", "Aircrafts/pmdg-aircraft-77w", kOtherDestination}};
    f.session.ShowActiveProfile();

    const std::vector<FileOperationResult> results = f.session.MoveAddons({AddonMove{kAddon, kSceneries}});

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists("D:/MSFS 2024/Sceneries/pmdg-aircraft-77w"));
    QCOMPARE(ComparablePath(f.settings.stored.profiles.front().destinationOverrides.front().relativePath),
             ComparablePath("Sceneries/pmdg-aircraft-77w"));
    QCOMPARE(f.observer.started, 2);
}

QTEST_MAIN(SessionTest)

#include "tst_session.moc"
