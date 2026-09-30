#include <QtTest/QtTest>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "application/StartupReport.h"
#include "domain/support/PathUtils.h"
#include "infrastructure/sim/ExeXmlStartupEntries.h"
#include "infrastructure/sim/StartupFileLocations.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/StdFilesystemProbe.h"
#include "tests/support/TempFiles.h"

namespace
{
    class StartupOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheEntriesComeBackFromTheFileOnDisk();
        static void TheSwitchLandsOnDiskAndTheNextReadSeesIt();
        static void TheFileIsRereadAtTheInstantOfWriting();
        static void TheBackupIsTheVersionImmediatelyBeforeTheWrite();
        static void NothingIsWrittenWhenTheBackupCannotBeMade();
        static void AFileThatIsNotThereIsRefusedBeforeAnythingIsWritten();
        static void AnEntryTheFileNoLongerCarriesIsRefusedAndNoBackupIsLeft();
        static void SwitchingAnEntryToTheValueItAlreadyHasWritesNothing();
        static void PointingItAtAnotherProfilesFileReadsAndWritesThatOne();
        static void TheEntryOfTheRealFileIsRecognisedAsCarriedByTheAddonItPointsInto();
        static void ABatchOfSwitchesLeavesTheBackupOfTheFileAsItWasBeforeTheFirstWrite();
        static void ASwitchOutsideABatchStillBacksUpItsOwnBefore();
        static void ASwitchAloneStillBacksUpItsOwnBeforeWhileABatchHoldsItsBackup();
        static void ABatchThatWritesNothingLeavesNoBackup();
        static void ABatchThatCouldNotBackUpTriesAgainOnTheNextWrite();
        static void ABatchWithARecordOfItsOwnBacksUpAgainAfterTheLastOne();
        static void PointingItAtAnotherFileWhileTheOtherIsBeingReadNeverMixesThemUp();
    };

    constexpr auto kAny2GSX = R"(C:\Users\pilot\AppData\Roaming\Any2GSX\bin\Any2GSX.exe)";
    constexpr auto kFsRealistic =
        R"(E:\Flight Simulator 2024\Community\rkapps-fsrealistic\service\FSRealistic-Plus.exe)";
    constexpr auto kFslControlCenter = R"(C:\FlightSimLabs\ControlCenter\FSLControlCenter.exe)";

    [[nodiscard]] std::string BytesOf(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);

        return std::string(std::istreambuf_iterator(stream), std::istreambuf_iterator<char>());
    }

    [[nodiscard]] std::string Fixture(const std::string& name)
    {
        return BytesOf(std::filesystem::path(FSORG_FIXTURES_DIR) / name);
    }

    [[nodiscard]] std::filesystem::path StartupFileIn(const TempFiles& files, const std::string& fixture)
    {
        const std::filesystem::path file = files.Root() / "EXE.xml";
        std::ofstream(file, std::ios::binary) << Fixture(fixture);

        return file;
    }

    [[nodiscard]] std::size_t FirstDifference(const std::string& left, const std::string& right)
    {
        const std::size_t shared = std::min(left.size(), right.size());

        for (std::size_t at = 0; at < shared; ++at)
        {
            if (left[at] != right[at])
            {
                return at;
            }
        }

        return left.size() == right.size() ? std::string::npos : shared;
    }

    [[nodiscard]] std::size_t HowManyFilesIn(const std::filesystem::path& folder)
    {
        return static_cast<std::size_t>(
            std::distance(std::filesystem::directory_iterator(folder), std::filesystem::directory_iterator()));
    }

    [[nodiscard]] std::string SignatureOf(const std::vector<StartupEntry>& entries)
    {
        std::string signature;
        for (const StartupEntry& entry : entries)
        {
            signature += entry.label + '|' + AsUtf8(entry.path) + '|' + (entry.enabled ? '1' : '0') + '\n';
        }

        return signature;
    }

    [[nodiscard]] bool EnabledIn(const std::vector<StartupEntry>& entries, const char* label)
    {
        for (const StartupEntry& entry : entries)
        {
            if (entry.label == label)
            {
                return entry.enabled;
            }
        }

        return false;
    }
}

void StartupOnRealDiskTest::TheEntriesComeBackFromTheFileOnDisk()
{
    const TempFiles files;
    const ExeXmlStartupEntries startup(StartupFileIn(files, "simulator-exe.xml"));

    const std::vector<StartupEntry> entries = startup.Entries();

    QCOMPARE(entries.size(), std::size_t{21});
    QVERIFY(!EnabledIn(entries, "Any2GSX"));
    QVERIFY(EnabledIn(entries, "FSRealistic+"));
}

void StartupOnRealDiskTest::TheSwitchLandsOnDiskAndTheNextReadSeesIt()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    ExeXmlStartupEntries startup(file);

    QCOMPARE(startup.Switch(PathFromUtf8(kAny2GSX), true), FileResult::Completed);

    QCOMPARE(FirstDifference(BytesOf(file), Fixture("simulator-exe-any2gsx-enabled.xml")), std::string::npos);
    QVERIFY(EnabledIn(startup.Entries(), "Any2GSX"));
}

void StartupOnRealDiskTest::TheFileIsRereadAtTheInstantOfWriting()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    ExeXmlStartupEntries startup(file);

    std::ofstream(file, std::ios::binary | std::ios::trunc) << Fixture("simulator-exe-any2gsx-enabled.xml");

    QVERIFY(EnabledIn(startup.Entries(), "Any2GSX"));
    QCOMPARE(startup.Switch(PathFromUtf8(kFsRealistic), false), FileResult::Completed);

    const std::vector<StartupEntry> entries = startup.Entries();

    QVERIFY(EnabledIn(entries, "Any2GSX"));
    QVERIFY(!EnabledIn(entries, "FSRealistic+"));
}

void StartupOnRealDiskTest::TheBackupIsTheVersionImmediatelyBeforeTheWrite()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    const std::filesystem::path backup = BackupOfStartupFile(file);
    ExeXmlStartupEntries startup(file);

    QCOMPARE(startup.Switch(PathFromUtf8(kAny2GSX), true), FileResult::Completed);
    QCOMPARE(FirstDifference(BytesOf(backup), Fixture("simulator-exe.xml")), std::string::npos);

    QCOMPARE(startup.Switch(PathFromUtf8(kFsRealistic), false), FileResult::Completed);
    QCOMPARE(FirstDifference(BytesOf(backup), Fixture("simulator-exe-any2gsx-enabled.xml")), std::string::npos);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{2});
}

void StartupOnRealDiskTest::NothingIsWrittenWhenTheBackupCannotBeMade()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    std::filesystem::create_directories(BackupOfStartupFile(file));
    ExeXmlStartupEntries startup(file);

    QCOMPARE(startup.Switch(PathFromUtf8(kAny2GSX), true), FileResult::CouldNotWriteTheStartupFile);
    QCOMPARE(FirstDifference(BytesOf(file), Fixture("simulator-exe.xml")), std::string::npos);
}

void StartupOnRealDiskTest::AFileThatIsNotThereIsRefusedBeforeAnythingIsWritten()
{
    const TempFiles files;
    ExeXmlStartupEntries startup(files.Root() / "EXE.xml");

    QVERIFY(startup.Entries().empty());
    QCOMPARE(startup.Switch(PathFromUtf8(kAny2GSX), true), FileResult::CouldNotReadTheStartupFile);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{0});
}

void StartupOnRealDiskTest::AnEntryTheFileNoLongerCarriesIsRefusedAndNoBackupIsLeft()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    ExeXmlStartupEntries startup(file);

    QCOMPARE(startup.Switch(PathFromUtf8(R"(C:\Nothing\Like\This\was-ever-installed.exe)"), true),
             FileResult::TheDiskDisagreesWithTheScan);
    QCOMPARE(FirstDifference(BytesOf(file), Fixture("simulator-exe.xml")), std::string::npos);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{1});
}

void StartupOnRealDiskTest::SwitchingAnEntryToTheValueItAlreadyHasWritesNothing()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    ExeXmlStartupEntries startup(file);

    QCOMPARE(startup.Switch(PathFromUtf8(kAny2GSX), false), FileResult::Completed);
    QCOMPARE(FirstDifference(BytesOf(file), Fixture("simulator-exe.xml")), std::string::npos);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{1});
}

void StartupOnRealDiskTest::PointingItAtAnotherProfilesFileReadsAndWritesThatOne()
{
    const TempFiles files;
    const std::filesystem::path other = files.Root() / "other";
    std::filesystem::create_directories(other);
    std::ofstream(other / "EXE.xml", std::ios::binary) << Fixture("simulator-exe-any2gsx-enabled.xml");

    ExeXmlStartupEntries startup(files.Root() / "EXE.xml");
    QVERIFY(startup.Entries().empty());

    startup.Use(other / "EXE.xml");

    QVERIFY(EnabledIn(startup.Entries(), "Any2GSX"));
    QCOMPARE(startup.Switch(PathFromUtf8(kAny2GSX), false), FileResult::Completed);
    QCOMPARE(FirstDifference(BytesOf(other / "EXE.xml"), Fixture("simulator-exe.xml")), std::string::npos);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{1});
}

void StartupOnRealDiskTest::TheEntryOfTheRealFileIsRecognisedAsCarriedByTheAddonItPointsInto()
{
    const TempFiles files;
    const ExeXmlStartupEntries startup(StartupFileIn(files, "simulator-exe.xml"));

    SimulatorProfile profile;
    profile.id = "msfs2024";
    profile.destinations = {PathFromUtf8(R"(E:\Flight Simulator 2024\Community)")};

    const StdFilesystemProbe probe;
    const StartupReport report = ReportStartupEntries(startup.Entries(), profile, ProfileSnapshot{}, probe);

    const std::vector<StartupLine> carried =
        EntriesCarriedBy(report, {PathFromUtf8(R"(D:\MSFS 2024\Utilities\rkapps-fsrealistic)")});

    QCOMPARE(carried.size(), std::size_t{1});
    QCOMPARE(carried.front().label, std::string("FSRealistic+"));
    QCOMPARE(carried.front().path, PathFromUtf8(kFsRealistic));
    QVERIFY(EntriesCarriedBy(report, {PathFromUtf8(R"(D:\MSFS 2024\Utilities\any2gsx)")}).empty());
}

void StartupOnRealDiskTest::ABatchOfSwitchesLeavesTheBackupOfTheFileAsItWasBeforeTheFirstWrite()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    const std::filesystem::path backup = BackupOfStartupFile(file);
    ExeXmlStartupEntries startup(file);

    StartupBackup batch;
    QCOMPARE(startup.Switch(PathFromUtf8(kAny2GSX), true, batch), FileResult::Completed);
    QCOMPARE(startup.Switch(PathFromUtf8(kFsRealistic), false, batch), FileResult::Completed);
    QCOMPARE(startup.Switch(PathFromUtf8(kFslControlCenter), false, batch), FileResult::Completed);

    const std::vector<StartupEntry> entries = startup.Entries();
    QVERIFY(EnabledIn(entries, "Any2GSX"));
    QVERIFY(!EnabledIn(entries, "FSRealistic+"));
    QVERIFY(!EnabledIn(entries, "FSL Control Center"));
    QVERIFY(batch.taken);
    QCOMPARE(FirstDifference(BytesOf(backup), Fixture("simulator-exe.xml")), std::string::npos);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{2});
}

void StartupOnRealDiskTest::ASwitchOutsideABatchStillBacksUpItsOwnBefore()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    const std::filesystem::path backup = BackupOfStartupFile(file);
    ExeXmlStartupEntries startup(file);

    QCOMPARE(startup.Switch(PathFromUtf8(kAny2GSX), true), FileResult::Completed);
    QCOMPARE(startup.Switch(PathFromUtf8(kFsRealistic), false), FileResult::Completed);
    QCOMPARE(startup.Switch(PathFromUtf8(kFslControlCenter), false), FileResult::Completed);

    const std::vector<StartupEntry> beforeTheLastSwitch = ExeXmlStartupEntries(backup).Entries();
    QVERIFY(EnabledIn(beforeTheLastSwitch, "Any2GSX"));
    QVERIFY(!EnabledIn(beforeTheLastSwitch, "FSRealistic+"));
    QVERIFY(EnabledIn(beforeTheLastSwitch, "FSL Control Center"));
}

void StartupOnRealDiskTest::ASwitchAloneStillBacksUpItsOwnBeforeWhileABatchHoldsItsBackup()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    const std::filesystem::path backup = BackupOfStartupFile(file);
    ExeXmlStartupEntries startup(file);

    StartupBackup batch;
    QCOMPARE(startup.Switch(PathFromUtf8(kAny2GSX), true, batch), FileResult::Completed);
    QVERIFY(batch.taken);
    QCOMPARE(FirstDifference(BytesOf(backup), Fixture("simulator-exe.xml")), std::string::npos);

    QCOMPARE(startup.Switch(PathFromUtf8(kFsRealistic), false), FileResult::Completed);

    QCOMPARE(FirstDifference(BytesOf(backup), Fixture("simulator-exe-any2gsx-enabled.xml")), std::string::npos);

    QCOMPARE(startup.Switch(PathFromUtf8(kFslControlCenter), false, batch), FileResult::Completed);

    QCOMPARE(FirstDifference(BytesOf(backup), Fixture("simulator-exe-any2gsx-enabled.xml")), std::string::npos);
}

void StartupOnRealDiskTest::ABatchThatWritesNothingLeavesNoBackup()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    ExeXmlStartupEntries startup(file);

    StartupBackup batch;
    QCOMPARE(startup.Switch(PathFromUtf8(kAny2GSX), false, batch), FileResult::Completed);
    QCOMPARE(startup.Switch(PathFromUtf8(kAny2GSX), false, batch), FileResult::Completed);

    QVERIFY(!batch.taken);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{1});
}

void StartupOnRealDiskTest::ABatchThatCouldNotBackUpTriesAgainOnTheNextWrite()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    const std::filesystem::path backup = BackupOfStartupFile(file);
    std::filesystem::create_directories(backup);
    ExeXmlStartupEntries startup(file);

    StartupBackup batch;
    QCOMPARE(startup.Switch(PathFromUtf8(kAny2GSX), true, batch), FileResult::CouldNotWriteTheStartupFile);
    QVERIFY(!batch.taken);
    QCOMPARE(FirstDifference(BytesOf(file), Fixture("simulator-exe.xml")), std::string::npos);

    std::filesystem::remove(backup);

    QCOMPARE(startup.Switch(PathFromUtf8(kFsRealistic), false, batch), FileResult::Completed);

    QCOMPARE(FirstDifference(BytesOf(backup), Fixture("simulator-exe.xml")), std::string::npos);
}

void StartupOnRealDiskTest::ABatchWithARecordOfItsOwnBacksUpAgainAfterTheLastOne()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    const std::filesystem::path backup = BackupOfStartupFile(file);
    ExeXmlStartupEntries startup(file);

    StartupBackup first;
    QCOMPARE(startup.Switch(PathFromUtf8(kAny2GSX), true, first), FileResult::Completed);
    QCOMPARE(FirstDifference(BytesOf(backup), Fixture("simulator-exe.xml")), std::string::npos);

    StartupBackup second;
    QCOMPARE(startup.Switch(PathFromUtf8(kFsRealistic), false, second), FileResult::Completed);
    QCOMPARE(startup.Switch(PathFromUtf8(kFslControlCenter), false, second), FileResult::Completed);

    QCOMPARE(FirstDifference(BytesOf(backup), Fixture("simulator-exe-any2gsx-enabled.xml")), std::string::npos);
}

void StartupOnRealDiskTest::PointingItAtAnotherFileWhileTheOtherIsBeingReadNeverMixesThemUp()
{
    const TempFiles files;
    const std::filesystem::path shortFolder = files.Root() / "a";
    const std::filesystem::path longFolder =
        files.Root() / "a-profile-folder-long-enough-to-leave-the-small-string-buffer-behind";
    std::filesystem::create_directories(shortFolder);
    std::filesystem::create_directories(longFolder);
    std::ofstream(shortFolder / "EXE.xml", std::ios::binary) << Fixture("simulator-exe.xml");
    std::ofstream(longFolder / "EXE.xml", std::ios::binary) << Fixture("simulator-exe-any2gsx-enabled.xml");

    const std::string ofTheShortOne = SignatureOf(ExeXmlStartupEntries(shortFolder / "EXE.xml").Entries());
    const std::string ofTheLongOne = SignatureOf(ExeXmlStartupEntries(longFolder / "EXE.xml").Entries());
    QVERIFY(!ofTheShortOne.empty());
    QVERIFY(ofTheShortOne != ofTheLongOne);

    ExeXmlStartupEntries startup(shortFolder / "EXE.xml");
    std::atomic<bool> reading = true;

    std::thread swapper(
        [&]
        {
            bool toTheLongOne = true;
            while (reading)
            {
                startup.Use((toTheLongOne ? longFolder : shortFolder) / "EXE.xml");
                toTheLongOne = !toTheLongOne;
            }
        });

    std::size_t strangers = 0;
    for (int round = 0; round < 3000; ++round)
    {
        const std::string answer = SignatureOf(startup.Entries());
        if (answer != ofTheShortOne && answer != ofTheLongOne)
        {
            ++strangers;
        }
    }

    reading = false;
    swapper.join();

    QCOMPARE(strangers, std::size_t{0});
}

QTEST_APPLESS_MAIN(StartupOnRealDiskTest)

#include "tst_startup_on_real_disk.moc"
