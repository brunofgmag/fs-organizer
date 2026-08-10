#include <QtTest/QtTest>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "application/StartupReport.h"
#include "domain/support/PathUtils.h"
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
        static void AnEnabledEntryWhoseExecutableIsGoneAlarms();
        static void AnEnabledEntryBehindAnAddonThatIsOffNowAlarmsAboutTheAddon();
        static void AnEnabledEntryThatOnlyPassesThroughADestinationDoesNotAlarm();
        static void ADisabledEntryDoesNotAlarmWhateverTheExecutableIsDoing();
        static void TheAlarmGoesAwayOnItsOwnWhenTheAddonComesBack();
        static void AnEntryOutsideEveryDestinationIsOutsideYourAddons();
        static void TheEntryInsideADestinationNamesTheFolderItReachesInto();
        static void AnExecutableMissingOutsideEveryDestinationIsMissingAndNotAnAddonThatIsOff();
        static void EveryEntryOfTheFileGetsALineAndTheOrderIsTheFileOrder();
        static void AnEnabledEntryReachingIntoTheAddonIsCarriedByIt();
        static void ADisabledEntryReachingIntoTheAddonIsNotCarriedByIt();
        static void AnEntryInAnotherAddonOfTheSameDestinationIsNotCarried();
        static void AnEntryOutsideEveryDestinationIsNotCarried();
        static void TheFolderNameIsMatchedWithoutMindingItsCase();
    };

    StartupReport ReportOf(const std::vector<StartupEntry>& entries, const FilesystemProbe& probe)
    {
        return ReportStartupEntries(entries, ProfileWithTheCommunity(), SnapshotHolding({}, {}), probe);
    }
}

void StartupReportTest::AnEnabledEntryWhoseExecutableIsGoneAlarms()
{
    InMemoryFileSystem disk;
    disk.AddDirectory(kCommunity);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report = ReportStartupEntries({Entry("FlowPro", kFlowExecutable, true)},
                                                      ProfileWithTheCommunity(), SnapshotHolding({}, {}), probe);

    QCOMPARE(report.lines.size(), std::size_t{1});
    QCOMPARE(report.lines.front().alarm, StartupAlarm::TheExecutableIsMissing);
}

void StartupReportTest::AnEnabledEntryBehindAnAddonThatIsOffNowAlarmsAboutTheAddon()
{
    InMemoryFileSystem disk;
    disk.AddDirectory(kCommunity);
    disk.AddDirectory(kFlowInTheLibrary);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report =
        ReportStartupEntries({Entry("FlowPro", kFlowExecutable, true)}, ProfileWithTheCommunity(),
                             SnapshotHolding({AddonNode(kFlowInTheLibrary)}, {}), probe);

    QCOMPARE(report.lines.size(), std::size_t{1});
    QCOMPARE(report.lines.front().alarm, StartupAlarm::TheAddonHoldingItIsOff);
    QCOMPARE(report.lines.front().addonFolder, kFlowInTheDestination);
}

void StartupReportTest::AnEnabledEntryThatOnlyPassesThroughADestinationDoesNotAlarm()
{
    InMemoryFileSystem disk;
    disk.AddFile(kFlowExecutable);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report =
        ReportStartupEntries({Entry("FlowPro", kFlowExecutable, true)}, ProfileWithTheCommunity(),
                             SnapshotHolding({AddonNode(kFlowInTheLibrary)}, {kFlowInTheLibrary}), probe);

    QCOMPARE(report.lines.size(), std::size_t{1});
    QCOMPARE(report.lines.front().alarm, StartupAlarm::None);
    QCOMPARE(report.lines.front().reach, StartupReach::InsideAnAddon);
}

void StartupReportTest::ADisabledEntryDoesNotAlarmWhateverTheExecutableIsDoing()
{
    InMemoryFileSystem disk;
    disk.AddDirectory(kCommunity);
    disk.AddDirectory(kFlowInTheLibrary);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report =
        ReportStartupEntries({Entry("FlowPro", kFlowExecutable, false), Entry("Simlink", kSimlink, false)},
                             ProfileWithTheCommunity(), SnapshotHolding({AddonNode(kFlowInTheLibrary)}, {}), probe);

    QCOMPARE(report.lines.size(), std::size_t{2});
    QCOMPARE(report.lines[0].alarm, StartupAlarm::None);
    QCOMPARE(report.lines[1].alarm, StartupAlarm::None);
    QCOMPARE(report.lines[0].reach, StartupReach::InsideAnAddon);
}

void StartupReportTest::TheAlarmGoesAwayOnItsOwnWhenTheAddonComesBack()
{
    InMemoryFileSystem disk;
    disk.AddDirectory(kCommunity);
    disk.AddDirectory(kFlowInTheLibrary);
    const FakeFilesystemProbe probe(disk);

    const std::vector<StartupEntry> entries = {Entry("FlowPro", kFlowExecutable, true)};
    const SimulatorProfile profile = ProfileWithTheCommunity();

    QCOMPARE(ReportStartupEntries(entries, profile, SnapshotHolding({AddonNode(kFlowInTheLibrary)}, {}), probe)
                 .lines.front()
                 .alarm,
             StartupAlarm::TheAddonHoldingItIsOff);

    disk.AddFile(kFlowExecutable);

    QCOMPARE(ReportStartupEntries(entries, profile,
                                  SnapshotHolding({AddonNode(kFlowInTheLibrary)}, {kFlowInTheLibrary}), probe)
                 .lines.front()
                 .alarm,
             StartupAlarm::None);
}

void StartupReportTest::AnEntryOutsideEveryDestinationIsOutsideYourAddons()
{
    InMemoryFileSystem disk;
    disk.AddFile(kSimlink);
    const FakeFilesystemProbe probe(disk);

    const StartupReport report = ReportStartupEntries({Entry("Navigraph Simlink", kSimlink, true)},
                                                      ProfileWithTheCommunity(), SnapshotHolding({}, {}), probe);

    QCOMPARE(report.lines.front().reach, StartupReach::OutsideYourAddons);
    QCOMPARE(report.lines.front().alarm, StartupAlarm::None);
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

    QCOMPARE(report.lines.front().alarm, StartupAlarm::TheExecutableIsMissing);
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

QTEST_APPLESS_MAIN(StartupReportTest)

#include "tst_startup_report.moc"
