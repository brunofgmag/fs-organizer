#include <QtTest/QtTest>

#include "application/Session.h"
#include "domain/journal/OperationLog.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/doubles/RecordingSessionObserver.h"
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
    static void RefreshingTheEntriesSeesALinkThatAppearedAfterTheScan();
    static void RefreshingTheEntriesSeesACopyThatAppearedAfterTheScan();
};

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kExtraLibrary = "D:/MSFS 2024 Extra";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kOtherDestination = "E:/Flight Simulator 2024/Community2024";
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
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        ProfileService service{catalog, classifier, linking, log, identities, LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        RecordingSessionObserver observer;
        Session session{service, settings, runner, observer};
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

    f.session.OverrideDestination(AddonNode(kAddon), kOtherDestination);

    QCOMPARE(f.session.Profile().destinationOverrides.size(), std::size_t{1});
    QCOMPARE(f.settings.stored.profiles.front().destinationOverrides.size(), std::size_t{1});
    QCOMPARE(f.observer.refreshed, 1);
    QCOMPARE(f.observer.finished, 1);
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

QTEST_MAIN(SessionTest)

#include "tst_session.moc"
