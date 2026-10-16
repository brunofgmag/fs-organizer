#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <unistd.h>

#include <fstream>
#include <string>
#include <vector>

#include "infrastructure/sim/ProcProcessProbe.h"
#include "support/PathText.h"

namespace
{
    class ProcProcessProbeTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AWindowsProgramUnderProtonIsFoundByTheNameOfItsExecutable();
        static void TheLauncherThatCarriesTheExecutableAmongItsArgumentsCountsToo();
        static void TheSimulatorOnADriveOfThePrefixIsFoundWhateverTheLetterAndTheNameWineGivesIt();
        static void TheNamesAreComparedWithoutCaseAndTheConfiguredSpellingIsTheAnswer();
        static void ATextThatOnlyMentionsTheNameIsNotTheProgram();
        static void AnotherProgramWhoseNameEndsTheSameIsNotTheProgram();
        static void NothingRunningAnswersNothing();
        static void ProcessesThatCannotBeReadAndFoldersThatAreNotProcessesAreSkipped();
        static void TheProbeNeverTakesItselfForTheSimulator();
        static void WithoutNamesNothingIsEverRunning();
        static void ARealProcessIsFoundThroughTheRealProc();
        static void NothingOnTheRealProcMatchesAnInventedName();
    };
}

namespace
{
    struct FakeProc
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return AsPath(directory.path());
        }

        void Process(const std::string& folder, const std::vector<std::string>& arguments) const
        {
            std::filesystem::create_directories(Root() / folder);
            std::ofstream cmdline(Root() / folder / "cmdline", std::ios::binary);

            for (const std::string& argument : arguments)
            {
                cmdline.write(argument.data(), static_cast<std::streamsize>(argument.size()));
                cmdline.put('\0');
            }
        }
    };

    const std::vector<std::string> kNames = {"FlightSimulator.exe", "FlightSimulator2024.exe"};
}

void ProcProcessProbeTest::AWindowsProgramUnderProtonIsFoundByTheNameOfItsExecutable()
{
    const FakeProc proc;
    proc.Process("812", {R"(Z:\home\pilot\.local\share\Steam\steamapps\common\MSFS2024\FlightSimulator2024.exe)"});
    proc.Process("813", {"/usr/bin/bash"});

    const ProcProcessProbe probe(kNames, proc.Root());

    QCOMPARE(probe.RunningSimulator(), std::optional<std::string>("FlightSimulator2024.exe"));
    QVERIFY(probe.SimulatorIsRunning());
}

void ProcProcessProbeTest::TheLauncherThatCarriesTheExecutableAmongItsArgumentsCountsToo()
{
    const FakeProc proc;
    proc.Process("77",
                 {"python3", "/home/pilot/.steam/compatibilitytools.d/proton", "waitforexitandrun",
                  "/home/pilot/.local/share/Steam/steamapps/common/MSFS2020/FlightSimulator.exe"});

    const ProcProcessProbe probe(kNames, proc.Root());

    QCOMPARE(probe.RunningSimulator(), std::optional<std::string>("FlightSimulator.exe"));
}

void ProcProcessProbeTest::TheSimulatorOnADriveOfThePrefixIsFoundWhateverTheLetterAndTheNameWineGivesIt()
{
    const FakeProc proc;
    proc.Process("4021", {"ACE: WinMain_Z"});
    proc.Process("4022",
                 {R"(S:\steamapps\common\MicrosoftFlightSimulator2024\FlightSimulator2024.exe)", "-FastLaunch"});
    proc.Process("4023", {"ACE: WinMain_Z", R"(s:\steamapps\common\MicrosoftFlightSimulator\flightsimulator.exe)"});

    const ProcProcessProbe probe(kNames, proc.Root());

    QCOMPARE(probe.RunningSimulator(), std::optional<std::string>("FlightSimulator2024.exe"));

    const FakeProc onlyTheSecond;
    onlyTheSecond.Process("4023",
                          {"ACE: WinMain_Z", R"(s:\steamapps\common\MicrosoftFlightSimulator\flightsimulator.exe)"});

    QCOMPARE(ProcProcessProbe(kNames, onlyTheSecond.Root()).RunningSimulator(),
             std::optional<std::string>("FlightSimulator.exe"));
}

void ProcProcessProbeTest::TheNamesAreComparedWithoutCaseAndTheConfiguredSpellingIsTheAnswer()
{
    const FakeProc proc;
    proc.Process("5", {R"(C:\Games\FLIGHTSIMULATOR.EXE)"});

    const ProcProcessProbe probe(kNames, proc.Root());

    QCOMPARE(probe.RunningSimulator(), std::optional<std::string>("FlightSimulator.exe"));
}

void ProcProcessProbeTest::ATextThatOnlyMentionsTheNameIsNotTheProgram()
{
    const FakeProc proc;
    proc.Process("9", {"grep", "-r", "FlightSimulator.exe", "."});
    proc.Process("10", {"tail", "-f", "FlightSimulator2024.exe"});

    const ProcProcessProbe probe(kNames, proc.Root());

    QVERIFY(!probe.RunningSimulator().has_value());
}

void ProcProcessProbeTest::AnotherProgramWhoseNameEndsTheSameIsNotTheProgram()
{
    const FakeProc proc;
    proc.Process("21", {R"(C:\Tools\NotFlightSimulator.exe)"});
    proc.Process("22", {"/opt/MyFlightSimulator2024.exe.bak"});

    const ProcProcessProbe probe(kNames, proc.Root());

    QVERIFY(!probe.RunningSimulator().has_value());
}

void ProcProcessProbeTest::NothingRunningAnswersNothing()
{
    const FakeProc proc;

    const ProcProcessProbe probe(kNames, proc.Root());

    QVERIFY(!probe.RunningSimulator().has_value());
    QVERIFY(!probe.SimulatorIsRunning());
}

void ProcProcessProbeTest::ProcessesThatCannotBeReadAndFoldersThatAreNotProcessesAreSkipped()
{
    const FakeProc proc;
    std::filesystem::create_directories(proc.Root() / "self");
    std::ofstream(proc.Root() / "self" / "cmdline") << "FlightSimulator.exe";
    std::filesystem::create_directories(proc.Root() / "300");
    std::filesystem::create_directories(proc.Root() / "301" / "cmdline");
    proc.Process("302", {});
    std::ofstream(proc.Root() / "version") << "Linux";
    proc.Process("303", {R"(Z:\sim\FlightSimulator.exe)"});

    const ProcProcessProbe probe(kNames, proc.Root());

    QCOMPARE(probe.RunningSimulator(), std::optional<std::string>("FlightSimulator.exe"));
}

void ProcProcessProbeTest::TheProbeNeverTakesItselfForTheSimulator()
{
    const FakeProc proc;
    proc.Process(std::to_string(::getpid()), {R"(Z:\sim\FlightSimulator.exe)"});

    const ProcProcessProbe probe(kNames, proc.Root());

    QVERIFY(!probe.RunningSimulator().has_value());
}

void ProcProcessProbeTest::WithoutNamesNothingIsEverRunning()
{
    const FakeProc proc;
    proc.Process("1", {R"(Z:\sim\FlightSimulator.exe)"});

    const ProcProcessProbe probe({}, proc.Root());

    QVERIFY(!probe.RunningSimulator().has_value());
}

void ProcProcessProbeTest::ARealProcessIsFoundThroughTheRealProc()
{
    QProcess sleeper;
    sleeper.start(QStringLiteral("sleep"), {QStringLiteral("30")});
    QVERIFY(sleeper.waitForStarted());

    const ProcProcessProbe probe({"sleep"});

    QCOMPARE(probe.RunningSimulator(), std::optional<std::string>("sleep"));

    sleeper.kill();
    sleeper.waitForFinished();

    QVERIFY(!probe.RunningSimulator().has_value());
}

void ProcProcessProbeTest::NothingOnTheRealProcMatchesAnInventedName()
{
    const ProcProcessProbe probe({"fsorg-invented-simulator-4f1c.exe"});

    QVERIFY(!probe.RunningSimulator().has_value());
}

QTEST_APPLESS_MAIN(ProcProcessProbeTest)

#include "tst_proc_process_probe.moc"
