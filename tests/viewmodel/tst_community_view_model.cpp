#include <QtTest/QtTest>

#include "application/LibraryOrganizer.h"
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
#include "viewmodel/CommunityViewModel.h"

class CommunityViewModelTest : public QObject
{
    class CommunityViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ShowingFillsTheTableFromTheSharedSnapshot();
        static void RepairingRemovesTheDeadRowsAndDropsTheAttentionCount();
        static void TheBreakdownSeparatesBrokenConflictedAndUnmanaged();
        static void TheBreakdownCountsAnAddonLinkedIntoTwoDestinations();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{path, Manifest{}};

        return node;
    }

    TreeNode LibraryTree()
    {
        TreeNode node;
        node.kind = TreeNodeKind::Library;
        node.path = kLibrary;
        node.children = {AddonNode("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w")};

        return node;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{"library-1", kLibrary, "MSFS 2024"}};

        return profile;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
            catalog.SetTree(kLibrary, LibraryTree());
        }

        void Seed(const SimulatorProfile& profile)
        {
            settings.stored.profiles = {profile};
            settings.stored.activeProfileId = profile.id;

            session.ShowActiveProfile();
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
        FakeFileOperations files{fileSystem};
        FakeProcessProbe processProbe;
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, processProbe, runner, notifier};
        CommunityModel model;
        CommunityViewModel viewModel{service, session, notifier, model};
    };
}

void CommunityViewModelTest::ShowingFillsTheTableFromTheSharedSnapshot()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/gone", "D:/Removed/gone");
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/physical");

    const SimulatorProfile profile = Profile();
    f.Seed(profile);

    f.viewModel.Show();

    QCOMPARE(f.model.rowCount({}), 2);
    QCOMPARE(f.viewModel.Breakdown().broken, std::size_t{1});
}

void CommunityViewModelTest::TheBreakdownSeparatesBrokenConflictedAndUnmanaged()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/gone", "D:/Removed/gone");
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/physical");

    f.Seed(Profile());
    f.viewModel.Show();

    const AttentionBreakdown breakdown = f.viewModel.Breakdown();

    QCOMPARE(breakdown.broken, std::size_t{1});
    QCOMPARE(breakdown.conflicts, std::size_t{0});
    QCOMPARE(breakdown.duplicated, std::size_t{0});
    QCOMPARE(breakdown.unmanaged, std::size_t{1});
}

void CommunityViewModelTest::TheBreakdownCountsAnAddonLinkedIntoTwoDestinations()
{
    constexpr auto kSecondDestination = "E:/Flight Simulator 2024/Community2024";
    constexpr auto kAddon = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";

    Fixture f;
    f.fileSystem.AddDirectory(kSecondDestination);
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", kAddon);
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w", kAddon);

    SimulatorProfile profile = Profile();
    profile.destinations = {kCommunity, kSecondDestination};

    f.Seed(profile);
    f.viewModel.Show();

    const AttentionBreakdown breakdown = f.viewModel.Breakdown();

    QCOMPARE(breakdown.duplicated, std::size_t{2});
    QCOMPARE(breakdown.broken, std::size_t{0});
    QCOMPARE(breakdown.unmanaged, std::size_t{0});
}

void CommunityViewModelTest::RepairingRemovesTheDeadRowsAndDropsTheAttentionCount()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/gone", "D:/Removed/gone");

    const SimulatorProfile profile = Profile();
    f.Seed(profile);
    f.viewModel.Show();

    const QSignalSpy finished(&f.viewModel, &CommunityViewModel::RepairFinished);
    const QSignalSpy attention(&f.viewModel, &CommunityViewModel::BreakdownChanged);

    std::vector<RepairRequest> requests;
    for (const RepairCandidate& candidate : f.viewModel.PlanRepairs())
    {
        requests.push_back({candidate, RepairAction::RemoveDeadNode});
    }

    f.viewModel.Repair(requests);

    QCOMPARE(finished.size(), 1);
    QCOMPARE(f.model.rowCount({}), 0);
    QCOMPARE(f.viewModel.Breakdown().broken, std::size_t{0});
    QCOMPARE(attention.size(), 1);
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/gone"));
}

QTEST_APPLESS_MAIN(CommunityViewModelTest)

#include "tst_community_view_model.moc"
