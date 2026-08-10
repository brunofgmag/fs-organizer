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
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/CommunityViewModel.h"

namespace
{
    class CommunityViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ShowingFillsTheTableFromTheSharedSnapshot();
        static void RepairingRemovesTheDeadRowsAndDropsTheAttentionCount();
        static void TheBreakdownSeparatesBrokenConflictedAndUnmanaged();
        static void TheBreakdownCountsAnAddonLinkedIntoTwoDestinations();
        static void AManagedEntryIsMeasuredAsTheAddonItPointsAtAndNeverAsTheLink();
        static void AnUnmanagedFolderIsMeasuredWhereItSitsBecauseThereIsNoLinkToFollow();
        static void AnUnavailableEntryIsNotMeasuredAndTheAnswerSaysWhatIsMissing();
        static void TwoEntriesPointingAtTheSameAddonCountItsBytesOnce();
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
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

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
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

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
        StartupOverFakes startup{filesystemProbe};

        ProfileService service{catalog, filesystemProbe, sidecars,        classifier,        linking,
                               log,     identities,      startup.service, LinkType::Junction};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeProcessProbe processProbe;
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, processProbe, runner, notifier};
        CommunityModel model;
        SizeService sizes{catalog, filesystemProbe, clock, runner};
        CommunityViewModel viewModel{service, session, notifier, model, sizes};
    };

    constexpr auto kAddonFolder = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";

    DestinationEntry Entry(const std::filesystem::path& path,
                           const std::filesystem::path& target,
                           const EntryClassification classification)
    {
        return DestinationEntry{.path = path, .target = target, .classification = classification};
    }

    SelectionSize LastSize(const QSignalSpy& measured)
    {
        return measured.isEmpty() ? SelectionSize{} : measured.back().front().value<SelectionSize>();
    }
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
        requests.push_back({.candidate = candidate, .action = RepairAction::RemoveDeadNode});
    }

    f.viewModel.Repair(requests);

    QCOMPARE(finished.size(), 1);
    QCOMPARE(f.model.rowCount({}), 0);
    QCOMPARE(f.viewModel.Breakdown().broken, std::size_t{0});
    QCOMPARE(attention.size(), 1);
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/gone"));
}

void CommunityViewModelTest::AManagedEntryIsMeasuredAsTheAddonItPointsAtAndNeverAsTheLink()
{
    Fixture f;
    const std::filesystem::path link = std::filesystem::path(kCommunity) / "pmdg-aircraft-77w";

    f.fileSystem.AddFile(std::filesystem::path(kAddonFolder) / "content.bin", 4096);

    const QSignalSpy measured(&f.viewModel, &CommunityViewModel::SizeMeasured);

    f.viewModel.MeasureTheSelection({Entry(link, kAddonFolder, EntryClassification::Managed)});

    QCOMPARE(measured.size(), 1);
    QCOMPARE(LastSize(measured).bytes, std::uintmax_t{4096});
    QCOMPARE(LastSize(measured).measured, std::size_t{1});
    QCOMPARE(LastSize(measured).selected, std::size_t{1});
    QCOMPARE(f.filesystemProbe.TimesWalked(kAddonFolder), std::size_t{1});
    QCOMPARE(f.filesystemProbe.TimesWalked(link), std::size_t{0});
}

void CommunityViewModelTest::AnUnmanagedFolderIsMeasuredWhereItSitsBecauseThereIsNoLinkToFollow()
{
    Fixture f;
    const std::filesystem::path physical = std::filesystem::path(kCommunity) / "physical";

    f.fileSystem.AddFile(physical / "content.bin", 700);

    const QSignalSpy measured(&f.viewModel, &CommunityViewModel::SizeMeasured);

    f.viewModel.MeasureTheSelection({Entry(physical, {}, EntryClassification::Unmanaged)});

    QCOMPARE(LastSize(measured).bytes, std::uintmax_t{700});
    QCOMPARE(f.filesystemProbe.TimesWalked(physical), std::size_t{1});
}

void CommunityViewModelTest::AnUnavailableEntryIsNotMeasuredAndTheAnswerSaysWhatIsMissing()
{
    Fixture f;
    const std::filesystem::path link = std::filesystem::path(kCommunity) / "pmdg-aircraft-77w";
    const std::filesystem::path adrift = std::filesystem::path(kCommunity) / "orbx-ybbn";

    f.fileSystem.AddFile(std::filesystem::path(kAddonFolder) / "content.bin", 4096);

    const QSignalSpy measured(&f.viewModel, &CommunityViewModel::SizeMeasured);

    f.viewModel.MeasureTheSelection({Entry(link, kAddonFolder, EntryClassification::Managed),
                                     Entry(adrift, "X:/Gone/orbx-ybbn", EntryClassification::Unavailable)});

    const SelectionSize size = LastSize(measured);

    QCOMPARE(size.bytes, std::uintmax_t{4096});
    QCOMPARE(size.measured, std::size_t{1});
    QCOMPARE(size.selected, std::size_t{2});
    QCOMPARE(size.unmeasured.size(), std::size_t{1});
    QCOMPARE(size.unmeasured.front().classification, EntryClassification::Unavailable);
    QCOMPARE(size.unmeasured.front().count, std::size_t{1});
    QCOMPARE(f.filesystemProbe.TimesWalked("X:/Gone/orbx-ybbn"), std::size_t{0});
    QCOMPARE(f.filesystemProbe.TimesWalked(adrift), std::size_t{0});
}

void CommunityViewModelTest::TwoEntriesPointingAtTheSameAddonCountItsBytesOnce()
{
    Fixture f;
    const std::filesystem::path here = std::filesystem::path(kCommunity) / "pmdg-aircraft-77w";
    const std::filesystem::path there = "E:/Flight Simulator 2024/Community2024/pmdg-aircraft-77w";

    f.fileSystem.AddFile(std::filesystem::path(kAddonFolder) / "content.bin", 4096);

    const QSignalSpy measured(&f.viewModel, &CommunityViewModel::SizeMeasured);

    f.viewModel.MeasureTheSelection({Entry(here, kAddonFolder, EntryClassification::Duplicated),
                                     Entry(there, kAddonFolder, EntryClassification::Duplicated)});

    const SelectionSize size = LastSize(measured);

    QCOMPARE(size.bytes, std::uintmax_t{4096});
    QCOMPARE(size.measured, std::size_t{2});
    QCOMPARE(size.selected, std::size_t{2});
    QCOMPARE(f.filesystemProbe.TimesWalked(kAddonFolder), std::size_t{1});
}

QTEST_APPLESS_MAIN(CommunityViewModelTest)

#include "tst_community_view_model.moc"
