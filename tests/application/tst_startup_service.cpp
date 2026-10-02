#include <QtTest/QtTest>

#include <cstddef>
#include <string>
#include <vector>

#include "application/StartupService.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeStartupEntries.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class StartupServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheSwitchGoesThroughWhenTheSimulatorIsNotRunning();
        static void TheSimulatorRunningRefusesTheWriteWithTheReasonThatBarsTheOthers();
        static void TheSimulatorRunningStillLetsTheEntriesBeRead();
        static void WithTheEntriesLeftLooseTheFileIsNeverRead();
        static void WithTheEntriesLeftLooseTheSwitchIsRefusedWithoutTouchingTheFile();
        static void TakingTheEntriesBackMakesTheFileReadableAgain();
        static void SwitchingAnEntryToTheValueItAlreadyHasWritesNothing();
        static void SwitchingAnEntryTheFileNoLongerCarriesWritesNothing();
        static void AnApplyGoesThroughWhenTheSimulatorIsClosedAndTheFeatureIsOn();
        static void AnApplyIsRefusedWithTheSimulatorRunningWhateverTheGestureThatTouchesTheStartupFile();
        static void ForgettingAnEntryNeverTouchesTheStartupFileSoTheSimulatorRunningDoesNotRefuseIt();
        static void AnApplyIsRefusedWhileTheEntriesAreLeftLooseWithoutTouchingTheFile();
        static void TheRemovedListIsReadWithTheSimulatorRunningAndIsEmptyWhileLeftLoose();
    };

    constexpr auto kFlowManager = R"(E:\Flight Simulator 2024\Community\p42-util-flow-pro\Flow for MSFS2024.exe)";

    void GiveItOneEntry(FakeStartupEntries& entries)
    {
        entries.Carry(StartupEntry{.label = "Flow Manager", .path = PathFromUtf8(kFlowManager), .enabled = true});
    }

    struct Disk
    {
        InMemoryFileSystem fileSystem{};
        FakeFilesystemProbe probe{fileSystem};
    };
}

void StartupServiceTest::TheSwitchGoesThroughWhenTheSimulatorIsNotRunning()
{
    FakeStartupEntries entries;
    GiveItOneEntry(entries);

    const FakeProcessProbe processProbe;
    const Disk disk;
    StartupService service(entries, processProbe, disk.probe, true);

    QCOMPARE(service.Switch(PathFromUtf8(kFlowManager), false), FileResult::Completed);
    QCOMPARE(entries.writes, std::size_t{1});
    QVERIFY(!service.Entries().front().enabled);
}

void StartupServiceTest::TheSimulatorRunningRefusesTheWriteWithTheReasonThatBarsTheOthers()
{
    FakeStartupEntries entries;
    GiveItOneEntry(entries);

    FakeProcessProbe processProbe;
    processProbe.ReportTheSimulatorAsRunning();
    const Disk disk;
    StartupService service(entries, processProbe, disk.probe, true);

    QCOMPARE(service.Switch(PathFromUtf8(kFlowManager), false), FileResult::TheSimulatorIsRunning);
    QCOMPARE(entries.writes, std::size_t{0});
    QVERIFY(service.Entries().front().enabled);
}

void StartupServiceTest::TheSimulatorRunningStillLetsTheEntriesBeRead()
{
    FakeStartupEntries entries;
    GiveItOneEntry(entries);

    FakeProcessProbe processProbe;
    processProbe.ReportTheSimulatorAsRunning();
    const Disk disk;
    const StartupService service(entries, processProbe, disk.probe, true);

    const std::vector<StartupEntry> read = service.Entries();

    QCOMPARE(read.size(), std::size_t{1});
    QCOMPARE(read.front().label, std::string("Flow Manager"));
}

void StartupServiceTest::WithTheEntriesLeftLooseTheFileIsNeverRead()
{
    FakeStartupEntries entries;
    GiveItOneEntry(entries);

    const FakeProcessProbe processProbe;
    const Disk disk;
    const StartupService service(entries, processProbe, disk.probe, false);

    QVERIFY(service.Entries().empty());
    QVERIFY(service.Report(SimulatorProfile{}, ProfileSnapshot{}).lines.empty());
    QCOMPARE(entries.reads, std::size_t{0});
}

void StartupServiceTest::WithTheEntriesLeftLooseTheSwitchIsRefusedWithoutTouchingTheFile()
{
    FakeStartupEntries entries;
    GiveItOneEntry(entries);

    const FakeProcessProbe processProbe;
    const Disk disk;
    StartupService service(entries, processProbe, disk.probe, false);

    QCOMPARE(service.Switch(PathFromUtf8(kFlowManager), false), FileResult::TheStartupEntriesAreLeftLoose);
    QCOMPARE(entries.writes, std::size_t{0});
    QCOMPARE(entries.reads, std::size_t{0});
}

void StartupServiceTest::TakingTheEntriesBackMakesTheFileReadableAgain()
{
    FakeStartupEntries entries;
    GiveItOneEntry(entries);

    const FakeProcessProbe processProbe;
    const Disk disk;
    StartupService service(entries, processProbe, disk.probe, false);

    QVERIFY(!service.Managing());

    service.Manage(true);

    QVERIFY(service.Managing());
    QCOMPARE(service.Entries().size(), std::size_t{1});
    QCOMPARE(service.Switch(PathFromUtf8(kFlowManager), false), FileResult::Completed);
}

void StartupServiceTest::SwitchingAnEntryToTheValueItAlreadyHasWritesNothing()
{
    FakeStartupEntries entries;
    GiveItOneEntry(entries);

    const FakeProcessProbe processProbe;
    const Disk disk;
    StartupService service(entries, processProbe, disk.probe, true);

    QCOMPARE(service.Switch(PathFromUtf8(kFlowManager), true), FileResult::Completed);
    QCOMPARE(entries.writes, std::size_t{0});
    QVERIFY(service.Entries().front().enabled);
}

void StartupServiceTest::SwitchingAnEntryTheFileNoLongerCarriesWritesNothing()
{
    FakeStartupEntries entries;
    GiveItOneEntry(entries);

    const FakeProcessProbe processProbe;
    const Disk disk;
    StartupService service(entries, processProbe, disk.probe, true);

    QCOMPARE(service.Switch(PathFromUtf8(R"(C:\Nothing\Like\This\was-ever-installed.exe)"), true),
             FileResult::TheDiskDisagreesWithTheScan);
    QCOMPARE(entries.writes, std::size_t{0});
}

void StartupServiceTest::AnApplyGoesThroughWhenTheSimulatorIsClosedAndTheFeatureIsOn()
{
    FakeStartupEntries entries;
    GiveItOneEntry(entries);

    const FakeProcessProbe processProbe;
    const Disk disk;
    StartupService service(entries, processProbe, disk.probe, true);
    StartupBackup backup;

    const StartupApplied applied = service.Apply(StartupRemoval{.path = PathFromUtf8(kFlowManager)}, backup);

    QCOMPARE(applied.result, FileResult::Completed);
    QVERIFY(applied.was.has_value());
    QCOMPARE(applied.was->label, std::string("Flow Manager"));
    QVERIFY(service.Entries().empty());
    QVERIFY(backup.taken);
}

void StartupServiceTest::AnApplyIsRefusedWithTheSimulatorRunningWhateverTheGestureThatTouchesTheStartupFile()
{
    FakeStartupEntries entries;
    GiveItOneEntry(entries);

    FakeProcessProbe processProbe;
    processProbe.ReportTheSimulatorAsRunning();
    const Disk disk;
    StartupService service(entries, processProbe, disk.probe, true);
    StartupBackup backup;

    const std::vector<StartupChange> gestures{
        StartupSwitching{.path = PathFromUtf8(kFlowManager), .enabled = false},
        StartupAddition{.label = "New", .path = PathFromUtf8(R"(C:\New\new.exe)")},
        StartupRemoval{.path = PathFromUtf8(kFlowManager)},
        StartupEditing{.path = PathFromUtf8(kFlowManager), .label = "x", .newPath = PathFromUtf8(kFlowManager)},
        StartupRestoring{.path = PathFromUtf8(kFlowManager)}};

    for (const StartupChange& gesture : gestures)
    {
        QCOMPARE(service.Apply(gesture, backup).result, FileResult::TheSimulatorIsRunning);
    }

    QCOMPARE(entries.writes, std::size_t{0});
    QCOMPARE(service.Entries().size(), std::size_t{1});
}

void StartupServiceTest::ForgettingAnEntryNeverTouchesTheStartupFileSoTheSimulatorRunningDoesNotRefuseIt()
{
    constexpr auto kGone = R"(C:\Gone\gone.exe)";

    FakeStartupEntries entries;
    GiveItOneEntry(entries);
    entries.CarryRemoved(StartupEntry{.label = "Gone", .path = PathFromUtf8(kGone)});

    FakeProcessProbe processProbe;
    processProbe.ReportTheSimulatorAsRunning();
    const Disk disk;
    StartupService service(entries, processProbe, disk.probe, true);
    StartupBackup backup;

    QCOMPARE(service.Apply(StartupForgetting{.path = PathFromUtf8(kGone)}, backup).result, FileResult::Completed);
    QVERIFY(service.Removed().empty());
    QCOMPARE(entries.writes, std::size_t{0});

    service.Manage(false);

    QCOMPARE(service.Apply(StartupForgetting{.path = PathFromUtf8(kGone)}, backup).result,
             FileResult::TheStartupEntriesAreLeftLoose);
}

void StartupServiceTest::AnApplyIsRefusedWhileTheEntriesAreLeftLooseWithoutTouchingTheFile()
{
    FakeStartupEntries entries;
    GiveItOneEntry(entries);

    const FakeProcessProbe processProbe;
    const Disk disk;
    StartupService service(entries, processProbe, disk.probe, false);
    StartupBackup backup;

    QCOMPARE(service.Apply(StartupRemoval{.path = PathFromUtf8(kFlowManager)}, backup).result,
             FileResult::TheStartupEntriesAreLeftLoose);
    QCOMPARE(entries.writes, std::size_t{0});
    QCOMPARE(entries.reads, std::size_t{0});
}

void StartupServiceTest::TheRemovedListIsReadWithTheSimulatorRunningAndIsEmptyWhileLeftLoose()
{
    FakeStartupEntries entries;
    entries.CarryRemoved(StartupEntry{.label = "Gone", .path = PathFromUtf8(R"(C:\Gone\gone.exe)")});

    FakeProcessProbe processProbe;
    processProbe.ReportTheSimulatorAsRunning();
    const Disk disk;
    StartupService service(entries, processProbe, disk.probe, true);

    QCOMPARE(service.Removed().size(), std::size_t{1});
    QCOMPARE(service.Removed().front().entry.label, std::string("Gone"));

    service.Manage(false);

    QVERIFY(service.Removed().empty());
}

QTEST_APPLESS_MAIN(StartupServiceTest)

#include "tst_startup_service.moc"
