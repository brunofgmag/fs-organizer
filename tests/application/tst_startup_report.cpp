#include <QtTest/QtTest>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "application/StartupReport.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    const std::filesystem::path kLibrary = "D:/MSFS 2024";
    const std::filesystem::path kCommunity = "E:/Flight Simulator 2024/Community";
    const std::filesystem::path kFlowInTheLibrary = kLibrary / "Utilities" / "p42-util-flow-pro";
    const std::filesystem::path kFlowInTheDestination = kCommunity / "p42-util-flow-pro";
    const std::filesystem::path kFlowExecutable = kFlowInTheDestination / "bin" / "flow.exe";
    const std::filesystem::path kSimlink = "C:/Program Files/Navigraph/Simlink/simlink.exe";

    SimulatorProfile ProfileWithTheCommunity()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
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

    ProfileSnapshot SnapshotHolding(std::vector<TreeNode> addons, const std::vector<std::filesystem::path>& enabled)
    {
        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = std::move(addons);

        ProfileSnapshot snapshot;
        snapshot.libraries.push_back(std::move(library));
        snapshot.enabled = EnabledAddons(enabled);

        return snapshot;
    }

    StartupEntry Entry(const std::string& label, const std::filesystem::path& path, const bool enabled)
    {
        return StartupEntry{.label = label, .path = path, .enabled = enabled};
    }

    class StartupReportTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnEnabledEntryWhoseExecutableIsGoneIsBroken();
        static void AnEnabledEntryBehindAnAddonThatIsOffNowIsBehindADisabledAddon();
        static void AnEnabledEntryThatOnlyPassesThroughADestinationIsReachable();
        static void ADisabledEntryStillReportsWhatIsWrongWithItsProgram();
        static void TheConditionFollowsTheAddonComingBack();
        static void AnEntryOutsideEveryDestinationIsOutsideYourAddons();
        static void TheEntryInsideADestinationNamesTheFolderItReachesInto();
        static void AnExecutableMissingOutsideEveryDestinationIsMissingAndNotAnAddonThatIsOff();
        static void EveryEntryOfTheFileGetsALineAndTheOrderIsTheFileOrder();
        static void AnEnabledEntryReachingIntoTheAddonIsCarriedByIt();
        static void ADisabledEntryReachingIntoTheAddonIsNotCarriedByIt();
        static void AnEntryInAnotherAddonOfTheSameDestinationIsNotCarried();
        static void AnEntryOutsideEveryDestinationIsNotCarried();
        static void TheFolderNameIsMatchedWithoutMindingItsCase();
        static void AReachableExecutableIsReachableWhateverTheSwitchSays();
        static void ABrokenExecutableIsBrokenEnabledOrNot();
        static void AnExecutableBehindAnAddonThatIsOffIsSaidSoEnabledOrNot();
        static void AnExecutableOnAVolumeThatIsNotMountedIsUnavailableAndNeverBroken();
        static void AnEnabledEntryOnAnUnavailableVolumeIsUnavailableAndStaysEnabled();
        static void TheLineCarriesTheCommandLineOfTheEntrySoTheEditCanOpenOnIt();
    };

    StartupReport ReportOf(const std::vector<StartupEntry>& entries, const FilesystemProbe& probe)
    {
        return ReportStartupEntries(entries, ProfileWithTheCommunity(), SnapshotHolding({}, {}), probe);
    }
}

void StartupReportTest::AnEnabledEntryWhoseExecutableIsGoneIsBroken()
{
    InMemoryFileSystem disk;
    disk.AddDirectory(kCommunity);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report = ReportStartupEntries({Entry("FlowPro", kFlowExecutable, true)},
                                                      ProfileWithTheCommunity(), SnapshotHolding({}, {}), probe);

    QCOMPARE(report.lines.size(), std::size_t{1});
    QCOMPARE(report.lines.front().condition, StartupCondition::Broken);
}

void StartupReportTest::AnEnabledEntryBehindAnAddonThatIsOffNowIsBehindADisabledAddon()
{
    InMemoryFileSystem disk;
    disk.AddDirectory(kCommunity);
    disk.AddDirectory(kFlowInTheLibrary);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report =
        ReportStartupEntries({Entry("FlowPro", kFlowExecutable, true)}, ProfileWithTheCommunity(),
                             SnapshotHolding({AddonNode(kFlowInTheLibrary)}, {}), probe);

    QCOMPARE(report.lines.size(), std::size_t{1});
    QCOMPARE(report.lines.front().condition, StartupCondition::BehindADisabledAddon);
    QCOMPARE(report.lines.front().addonFolder, kFlowInTheDestination);
}

void StartupReportTest::AnEnabledEntryThatOnlyPassesThroughADestinationIsReachable()
{
    InMemoryFileSystem disk;
    disk.AddFile(kFlowExecutable);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report =
        ReportStartupEntries({Entry("FlowPro", kFlowExecutable, true)}, ProfileWithTheCommunity(),
                             SnapshotHolding({AddonNode(kFlowInTheLibrary)}, {kFlowInTheLibrary}), probe);

    QCOMPARE(report.lines.size(), std::size_t{1});
    QCOMPARE(report.lines.front().condition, StartupCondition::Reachable);
    QCOMPARE(report.lines.front().reach, StartupReach::InsideAnAddon);
}

void StartupReportTest::ADisabledEntryStillReportsWhatIsWrongWithItsProgram()
{
    InMemoryFileSystem disk;
    disk.AddDirectory(kCommunity);
    disk.AddDirectory(kFlowInTheLibrary);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report =
        ReportStartupEntries({Entry("FlowPro", kFlowExecutable, false), Entry("Simlink", kSimlink, false)},
                             ProfileWithTheCommunity(), SnapshotHolding({AddonNode(kFlowInTheLibrary)}, {}), probe);

    QCOMPARE(report.lines.size(), std::size_t{2});
    QCOMPARE(report.lines[0].condition, StartupCondition::BehindADisabledAddon);
    QCOMPARE(report.lines[1].condition, StartupCondition::Broken);
    QVERIFY(!report.lines[0].enabled);
    QVERIFY(!report.lines[1].enabled);
    QCOMPARE(report.lines[0].reach, StartupReach::InsideAnAddon);
}

void StartupReportTest::TheConditionFollowsTheAddonComingBack()
{
    InMemoryFileSystem disk;
    disk.AddDirectory(kCommunity);
    disk.AddDirectory(kFlowInTheLibrary);
    const FakeFilesystemProbe probe(disk);

    const std::vector<StartupEntry> entries = {Entry("FlowPro", kFlowExecutable, true)};
    const SimulatorProfile profile = ProfileWithTheCommunity();

    QCOMPARE(ReportStartupEntries(entries, profile, SnapshotHolding({AddonNode(kFlowInTheLibrary)}, {}), probe)
                 .lines.front()
                 .condition,
             StartupCondition::BehindADisabledAddon);

    disk.AddFile(kFlowExecutable);

    QCOMPARE(ReportStartupEntries(entries, profile,
                                  SnapshotHolding({AddonNode(kFlowInTheLibrary)}, {kFlowInTheLibrary}), probe)
                 .lines.front()
                 .condition,
             StartupCondition::Reachable);
}

void StartupReportTest::AnEntryOutsideEveryDestinationIsOutsideYourAddons()
{
    InMemoryFileSystem disk;
    disk.AddFile(kSimlink);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report = ReportStartupEntries({Entry("Navigraph Simlink", kSimlink, true)},
                                                      ProfileWithTheCommunity(), SnapshotHolding({}, {}), probe);

    QCOMPARE(report.lines.front().reach, StartupReach::OutsideYourAddons);
    QCOMPARE(report.lines.front().condition, StartupCondition::Reachable);
    QVERIFY(report.lines.front().addonFolder.empty());
}

void StartupReportTest::TheEntryInsideADestinationNamesTheFolderItReachesInto()
{
    InMemoryFileSystem disk;
    disk.AddFile(kFlowExecutable);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report = ReportStartupEntries({Entry("FlowPro", kFlowExecutable, true)},
                                                      ProfileWithTheCommunity(), SnapshotHolding({}, {}), probe);

    QCOMPARE(report.lines.front().reach, StartupReach::InsideAnAddon);
    QCOMPARE(report.lines.front().addonFolder, kFlowInTheDestination);
}

void StartupReportTest::AnExecutableMissingOutsideEveryDestinationIsMissingAndNotAnAddonThatIsOff()
{
    InMemoryFileSystem disk;
    disk.AddDirectory(kFlowInTheLibrary);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report = ReportStartupEntries({Entry("Simlink", kSimlink, true)}, ProfileWithTheCommunity(),
                                                      SnapshotHolding({AddonNode(kFlowInTheLibrary)}, {}), probe);

    QCOMPARE(report.lines.front().condition, StartupCondition::Broken);
    QCOMPARE(report.lines.front().reach, StartupReach::OutsideYourAddons);
}

void StartupReportTest::EveryEntryOfTheFileGetsALineAndTheOrderIsTheFileOrder()
{
    InMemoryFileSystem disk;
    disk.AddFile(kFlowExecutable);
    disk.AddFile(kSimlink);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report =
        ReportStartupEntries({Entry("FlowPro", kFlowExecutable, true), Entry("Navigraph Simlink", kSimlink, true),
                              Entry("GSX Pro", "C:/Program Files/Addon Manager/couatl64/couatl64_MSFS.exe", true)},
                             ProfileWithTheCommunity(), SnapshotHolding({}, {}), probe);

    QCOMPARE(report.lines.size(), std::size_t{3});
    QCOMPARE(report.lines[0].label, std::string("FlowPro"));
    QCOMPARE(report.lines[1].label, std::string("Navigraph Simlink"));
    QCOMPARE(report.lines[2].label, std::string("GSX Pro"));
    QCOMPARE(report.lines[0].reach, StartupReach::InsideAnAddon);
    QCOMPARE(report.lines[1].reach, StartupReach::OutsideYourAddons);
    QCOMPARE(report.lines[2].reach, StartupReach::OutsideYourAddons);
}

void StartupReportTest::AnEnabledEntryReachingIntoTheAddonIsCarriedByIt()
{
    InMemoryFileSystem disk;
    disk.AddFile(kFlowExecutable);
    const FakeFilesystemProbe probe(disk);

    const std::vector<StartupLine> carried =
        EntriesCarriedBy(ReportOf({Entry("FlowPro", kFlowExecutable, true)}, probe), {kFlowInTheLibrary});

    QCOMPARE(carried.size(), std::size_t{1});
    QCOMPARE(carried.front().label, std::string("FlowPro"));
    QCOMPARE(carried.front().path, kFlowExecutable);
}

void StartupReportTest::ADisabledEntryReachingIntoTheAddonIsNotCarriedByIt()
{
    InMemoryFileSystem disk;
    disk.AddFile(kFlowExecutable);
    const FakeFilesystemProbe probe(disk);

    const std::vector<StartupLine> carried =
        EntriesCarriedBy(ReportOf({Entry("FlowPro", kFlowExecutable, false)}, probe), {kFlowInTheLibrary});

    QVERIFY(carried.empty());
}

void StartupReportTest::AnEntryInAnotherAddonOfTheSameDestinationIsNotCarried()
{
    const std::filesystem::path otherExecutable = kCommunity / "fbw-aircraft-a320" / "bin" / "sync.exe";

    InMemoryFileSystem disk;
    disk.AddFile(otherExecutable);
    const FakeFilesystemProbe probe(disk);

    const std::vector<StartupLine> carried =
        EntriesCarriedBy(ReportOf({Entry("A320 Sync", otherExecutable, true)}, probe), {kFlowInTheLibrary});

    QVERIFY(carried.empty());
}

void StartupReportTest::AnEntryOutsideEveryDestinationIsNotCarried()
{
    InMemoryFileSystem disk;
    disk.AddFile(kSimlink);
    const FakeFilesystemProbe probe(disk);

    const std::vector<StartupLine> carried =
        EntriesCarriedBy(ReportOf({Entry("Navigraph Simlink", kSimlink, true)}, probe), {kFlowInTheLibrary});

    QVERIFY(carried.empty());
}

void StartupReportTest::TheFolderNameIsMatchedWithoutMindingItsCase()
{
    InMemoryFileSystem disk;
    disk.AddFile(kFlowExecutable);
    const FakeFilesystemProbe probe(disk);

    const std::vector<StartupLine> carried = EntriesCarriedBy(
        ReportOf({Entry("FlowPro", kFlowExecutable, true)}, probe), {kLibrary / "Utilities" / "P42-Util-Flow-Pro"});

    QCOMPARE(carried.size(), std::size_t{1});
}

void StartupReportTest::AReachableExecutableIsReachableWhateverTheSwitchSays()
{
    InMemoryFileSystem disk;
    disk.AddFile(kFlowExecutable);
    disk.AddFile(kSimlink);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report =
        ReportOf({Entry("FlowPro", kFlowExecutable, true), Entry("Simlink", kSimlink, false)}, probe);

    QCOMPARE(report.lines[0].condition, StartupCondition::Reachable);
    QCOMPARE(report.lines[1].condition, StartupCondition::Reachable);
}

void StartupReportTest::ABrokenExecutableIsBrokenEnabledOrNot()
{
    InMemoryFileSystem disk;
    disk.AddDirectory(kCommunity);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report =
        ReportOf({Entry("FlowPro", kFlowExecutable, true), Entry("Simlink", kSimlink, false)}, probe);

    QCOMPARE(report.lines[0].condition, StartupCondition::Broken);
    QCOMPARE(report.lines[1].condition, StartupCondition::Broken);
}

void StartupReportTest::AnExecutableBehindAnAddonThatIsOffIsSaidSoEnabledOrNot()
{
    InMemoryFileSystem disk;
    disk.AddDirectory(kCommunity);
    disk.AddDirectory(kFlowInTheLibrary);
    const FakeFilesystemProbe probe(disk);

    for (const bool enabled : {true, false})
    {
        const StartupReport report =
            ReportStartupEntries({Entry("FlowPro", kFlowExecutable, enabled)}, ProfileWithTheCommunity(),
                                 SnapshotHolding({AddonNode(kFlowInTheLibrary)}, {}), probe);

        QCOMPARE(report.lines.front().condition, StartupCondition::BehindADisabledAddon);
    }
}

void StartupReportTest::AnExecutableOnAVolumeThatIsNotMountedIsUnavailableAndNeverBroken()
{
    InMemoryFileSystem disk;
    disk.MarkVolumeUnavailable(kSimlink);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report =
        ReportOf({Entry("Simlink", kSimlink, false), Entry("Other", "C:/Other/other.exe", false)}, probe);

    QCOMPARE(report.lines[0].condition, StartupCondition::Unavailable);
    QCOMPARE(report.lines[1].condition, StartupCondition::Unavailable);
}

void StartupReportTest::AnEnabledEntryOnAnUnavailableVolumeIsUnavailableAndStaysEnabled()
{
    InMemoryFileSystem disk;
    disk.MarkVolumeUnavailable(kSimlink);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report = ReportOf({Entry("Simlink", kSimlink, true)}, probe);

    QCOMPARE(report.lines.front().condition, StartupCondition::Unavailable);
    QVERIFY(report.lines.front().enabled);
}

void StartupReportTest::TheLineCarriesTheCommandLineOfTheEntrySoTheEditCanOpenOnIt()
{
    InMemoryFileSystem disk;
    disk.AddFile(kSimlink);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report = ReportOf(
        {StartupEntry{.label = "Simlink", .path = kSimlink, .commandLine = "-minimized \"a b\"", .enabled = true}},
        probe);

    QCOMPARE(report.lines.front().commandLine, std::string("-minimized \"a b\""));
}

QTEST_APPLESS_MAIN(StartupReportTest)

#include "tst_startup_report.moc"
