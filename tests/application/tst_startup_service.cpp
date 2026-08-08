#include <QtTest/QtTest>

#include <cstddef>
#include <filesystem>
#include <vector>

#include "application/StartupService.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeStartupEntries.h"
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
    };

    constexpr auto kFlowManager = R"(E:\Flight Simulator 2024\Community\p42-util-flow-pro\Flow for MSFS2024.exe)";

    void GiveItOneEntry(FakeStartupEntries& entries)
    {
        entries.Carry(StartupEntry{.label = "Flow Manager", .path = PathFromUtf8(kFlowManager), .enabled = true});
    }
}

void StartupServiceTest::TheSwitchGoesThroughWhenTheSimulatorIsNotRunning()
{
    FakeStartupEntries entries;
    GiveItOneEntry(entries);

    const FakeProcessProbe processProbe;
    StartupService service(entries, processProbe);

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
    StartupService service(entries, processProbe);

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
    const StartupService service(entries, processProbe);

    const std::vector<StartupEntry> read = service.Entries();

    QCOMPARE(read.size(), std::size_t{1});
    QCOMPARE(read.front().label, std::string("Flow Manager"));
}

QTEST_APPLESS_MAIN(StartupServiceTest)

#include "tst_startup_service.moc"
