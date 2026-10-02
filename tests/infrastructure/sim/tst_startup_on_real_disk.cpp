#include <QtTest/QtTest>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <string_view>
#include <thread>
#include <vector>

#include "application/StartupReport.h"
#include "domain/support/PathUtils.h"
#include "infrastructure/sim/ExeXmlStartupEntries.h"
#include "infrastructure/sim/RemovedStartupEntriesFile.h"
#include "infrastructure/sim/StartupFileLocations.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/StdFilesystemProbe.h"
#include "tests/support/TempFiles.h"
#include "tests/support/Utf16Text.h"

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

        static void RemovingAnEntryKeepsItsBlockInTheRemovedFileAndThenWritesTheFile();
        static void AGuardThatFailsLeavesTheStartupFileAndItsBackupUntouched();
        static void ARestoredEntryComesBackByteForByteAndLeavesTheRemovedList();
        static void TheRemovedListSurvivesANewAdapterOnTheSameFiles();
        static void ARemovedEntryWhoseProgramIsBackInTheFileIsNotListedAsRemoved();
        static void RemovingAPathThatIsAlreadyKeptListsTheNewestAndKeepsTheOlderUnderIt();
        static void ForgettingAnEntryOnlyTakesItOutOfTheRemovedList();
        static void TheFileIsCreatedBesideAUserCfgAndOnlyThen();
        static void NothingIsCreatedWhereThereIsNoUserCfgBesideTheFile();
        static void OnlyAnAdditionCreatesTheFileEvenBesideAUserCfg();
        static void ABatchOfNewOperationsLeavesTheBackupAsItWasBeforeTheFirstWrite();
        static void ARefusedChangeLeavesNoBackupAndNoFileBehind();
        static void AnEditThatMovesTheEntryKeepsTheOldBlockAndOneThatDoesNotKeepsNothing();
        static void AWindows1252FileKeepsItsBytesThroughARemovalAndARestore();
        static void AnAdditionTheFileCannotHoldWritesNothing();
        static void AnEmptyFilePathNeverLooksForUserCfgInTheCurrentFolder();
        static void AUtf16FileIsReadButEveryWriteIsRefusedAndNothingIsLeftBehind();
        static void TheRemovedFileIsNamedByTheSimulatorVariantAndCreatesItsFolder();
        static void ARemovedEntryIsStampedWithTheInstantOfTheEditThatMovedIt();
        static void RemovalsWithTheSameInstantListTheLastRemovedFirst();
        static void AFileThatExistsButCannotBeReadIsNeverStartedOverByAnAddition();
        static void TwoChangesAtTheSameTimeBothLandInTheFile();
        static void AChangeTheLocatorAndTheReaderDisagreeAboutIsRefusedAndWritesNothing();
        static void ANamelessEntryNamedAndEmptiedThroughTheAdapterLeavesWellFormedXml();
        static void AnEditToWhatTheEntryHoldsSaysNothingChangedAndWritesNothing();
    };

    constexpr auto kAny2GSX = R"(C:\Users\pilot\AppData\Roaming\Any2GSX\bin\Any2GSX.exe)";
    constexpr auto kFsRealistic =
        R"(E:\Flight Simulator 2024\Community\rkapps-fsrealistic\service\FSRealistic-Plus.exe)";
    constexpr auto kFslControlCenter = R"(C:\FlightSimLabs\ControlCenter\FSLControlCenter.exe)";

    constexpr auto kIFly = R"(E:\Flight Simulator 2024\Community\ifly-aircraft-737max8\Data\Tool\737MAX_Plugin.exe)";
    constexpr auto kDynamicLod =
        R"(C:\Users\pilot\AppData\Roaming\DynamicLOD_ResetEdition\bin\DynamicLOD_ResetEdition.exe)";
    constexpr auto kFenix = R"(C:\Program Files\FenixSim A320\deps\FenixBootstrapper.exe)";

    constexpr std::string_view kAccentedFileIn1252 = "<?xml version=\"1.0\" encoding=\"windows-1252\"?>\r\n"
                                                     "<SimBase.Document Type=\"Launch\" version=\"1,0\">\r\n"
                                                     "  <Launch.Addon>\r\n"
                                                     "    <Name>Avi\xF5"
                                                     "es</Name>\r\n"
                                                     "    <Disabled>False</Disabled>\r\n"
                                                     "    <Path>C:\\Avi\xF5"
                                                     "es\\x.exe</Path>\r\n"
                                                     "    <CommandLine>--m\xE3"
                                                     "o</CommandLine>\r\n"
                                                     "  </Launch.Addon>\r\n"
                                                     "  <Launch.Addon>\r\n"
                                                     "    <Name>Cr\xE9"
                                                     "dito</Name>\r\n"
                                                     "    <Disabled>True</Disabled>\r\n"
                                                     "    <Path>C:\\Cr\xE9"
                                                     "dito\\y.exe</Path>\r\n"
                                                     "  </Launch.Addon>\r\n"
                                                     "</SimBase.Document>\r\n";

    [[nodiscard]] std::chrono::system_clock::time_point Moment(const long long milliseconds)
    {
        return std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::milliseconds(milliseconds)));
    }

    constexpr std::string_view kUtf16Document = "<?xml version=\"1.0\" encoding=\"utf-16\"?>\r\n"
                                                "<SimBase.Document Type=\"Launch\" version=\"1,0\">\r\n"
                                                "  <Launch.Addon>\r\n"
                                                "    <Name>Avi\xC3\xB5"
                                                "es</Name>\r\n"
                                                "    <Disabled>False</Disabled>\r\n"
                                                "    <Path>C:\\Avi\xC3\xB5"
                                                "es\\x.exe</Path>\r\n"
                                                "  </Launch.Addon>\r\n"
                                                "</SimBase.Document>\r\n";

    [[nodiscard]] std::filesystem::path RemovedFileIn(const TempFiles& files)
    {
        return files.Root() / "removed.json";
    }

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

void StartupOnRealDiskTest::RemovingAnEntryKeepsItsBlockInTheRemovedFileAndThenWritesTheFile()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    ExeXmlStartupEntries startup(file);
    startup.KeepRemovedEntriesIn(RemovedFileIn(files));

    StartupBackup backup;
    const StartupApplied applied = startup.Apply(StartupRemoval{.path = PathFromUtf8(kIFly)}, backup);

    QCOMPARE(applied.result, FileResult::Completed);
    QVERIFY(applied.was.has_value());
    QCOMPARE(applied.was->label, std::string("iFly Plugin"));

    const std::vector<RemovedStartupEntry> kept = RemovedStartupEntriesFile(RemovedFileIn(files)).Entries();

    QCOMPARE(kept.size(), std::size_t{1});
    QCOMPARE(kept.front().path, PathFromUtf8(kIFly));
    QCOMPARE(kept.front().label, std::string("iFly Plugin"));
    QCOMPARE(kept.front().commandLine, std::string("auto"));
    QCOMPARE(kept.front().followedBy, PathFromUtf8(kDynamicLod));

    std::string expected = Fixture("simulator-exe.xml");
    const std::size_t at = expected.find(kept.front().block);

    QVERIFY(at != std::string::npos);
    QVERIFY(kept.front().block.find("<ManualLoad>False</ManualLoad>") != std::string::npos);
    QVERIFY(kept.front().block.find("<NewConsole>False</NewConsole>") != std::string::npos);

    expected.erase(at, kept.front().block.size());

    QCOMPARE(FirstDifference(BytesOf(file), expected), std::string::npos);
    QCOMPARE(FirstDifference(BytesOf(BackupOfStartupFile(file)), Fixture("simulator-exe.xml")), std::string::npos);
}

void StartupOnRealDiskTest::AGuardThatFailsLeavesTheStartupFileAndItsBackupUntouched()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    ExeXmlStartupEntries startup(file);

    const StartupRemoval removing{.path = PathFromUtf8(kIFly)};
    const StartupEditing moving{.path = PathFromUtf8(kIFly),
                                .label = "iFly Plugin",
                                .newPath = PathFromUtf8(R"(C:\Elsewhere\iFly.exe)"),
                                .commandLine = "auto"};

    StartupBackup backup;

    QCOMPARE(startup.Apply(removing, backup).result, FileResult::CouldNotKeepTheRemovedEntry);
    QCOMPARE(startup.Apply(moving, backup).result, FileResult::CouldNotKeepTheRemovedEntry);

    startup.KeepRemovedEntriesIn(file / "removed.json");

    QCOMPARE(startup.Apply(removing, backup).result, FileResult::CouldNotKeepTheRemovedEntry);
    QCOMPARE(startup.Apply(moving, backup).result, FileResult::CouldNotKeepTheRemovedEntry);

    static_cast<void>(files.WriteText("removed.json", "this is not json"));
    startup.KeepRemovedEntriesIn(RemovedFileIn(files));

    QCOMPARE(startup.Apply(removing, backup).result, FileResult::CouldNotKeepTheRemovedEntry);
    QCOMPARE(startup.Apply(moving, backup).result, FileResult::CouldNotKeepTheRemovedEntry);

    QCOMPARE(BytesOf(RemovedFileIn(files)), std::string("this is not json"));
    QCOMPARE(FirstDifference(BytesOf(file), Fixture("simulator-exe.xml")), std::string::npos);
    QVERIFY(!backup.taken);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{2});
}

void StartupOnRealDiskTest::ARestoredEntryComesBackByteForByteAndLeavesTheRemovedList()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    ExeXmlStartupEntries startup(file);
    startup.KeepRemovedEntriesIn(RemovedFileIn(files));

    for (const char* path : {kAny2GSX, kIFly, kFsRealistic})
    {
        StartupBackup backup;

        QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(path)}, backup).result, FileResult::Completed);
        QVERIFY(FirstDifference(BytesOf(file), Fixture("simulator-exe.xml")) != std::string::npos);
        QCOMPARE(startup.Removed().size(), std::size_t{1});

        QCOMPARE(startup.Apply(StartupRestoring{.path = PathFromUtf8(path)}, backup).result, FileResult::Completed);
        QCOMPARE(FirstDifference(BytesOf(file), Fixture("simulator-exe.xml")), std::string::npos);
        QVERIFY(startup.Removed().empty());
        QVERIFY(RemovedStartupEntriesFile(RemovedFileIn(files)).Entries().empty());
    }
}

void StartupOnRealDiskTest::TheRemovedListSurvivesANewAdapterOnTheSameFiles()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");

    {
        ExeXmlStartupEntries startup(file);
        startup.KeepRemovedEntriesIn(RemovedFileIn(files));

        StartupBackup backup;

        QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(kIFly), .at = Moment(200'123)}, backup).result,
                 FileResult::Completed);
        QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(kFslControlCenter), .at = Moment(100'000)}, backup)
                     .result,
                 FileResult::Completed);
    }

    ExeXmlStartupEntries again(file);

    QVERIFY(again.Removed().empty());

    again.KeepRemovedEntriesIn(RemovedFileIn(files));

    const std::vector<StartupRemovedEntry> removed = again.Removed();

    QCOMPARE(removed.size(), std::size_t{2});
    QCOMPARE(removed[0].entry.label, std::string("iFly Plugin"));
    QCOMPARE(removed[0].entry.path, PathFromUtf8(kIFly));
    QCOMPARE(removed[0].entry.commandLine, std::string("auto"));
    QVERIFY(removed[0].entry.enabled);
    QCOMPARE(removed[0].removedAt, Moment(200'123));
    QCOMPARE(removed[1].entry.label, std::string("FSL Control Center"));
    QCOMPARE(removed[1].entry.commandLine, std::string("-NOTIFY"));
    QCOMPARE(removed[1].removedAt, Moment(100'000));

    StartupBackup backup;

    QCOMPARE(again.Apply(StartupRestoring{.path = PathFromUtf8(kIFly)}, backup).result, FileResult::Completed);
    QCOMPARE(again.Removed().size(), std::size_t{1});
}

void StartupOnRealDiskTest::ARemovedEntryWhoseProgramIsBackInTheFileIsNotListedAsRemoved()
{
    const TempFiles files;
    ExeXmlStartupEntries startup(StartupFileIn(files, "simulator-exe.xml"));
    startup.KeepRemovedEntriesIn(RemovedFileIn(files));

    StartupBackup backup;

    QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(kIFly)}, backup).result, FileResult::Completed);
    QCOMPARE(startup.Removed().size(), std::size_t{1});

    QCOMPARE(startup.Apply(StartupAddition{.label = "Typed again", .path = PathFromUtf8(kIFly)}, backup).result,
             FileResult::Completed);

    QVERIFY(startup.Removed().empty());
    QCOMPARE(RemovedStartupEntriesFile(RemovedFileIn(files)).Entries().size(), std::size_t{1});
    QCOMPARE(startup.Apply(StartupRestoring{.path = PathFromUtf8(kIFly)}, backup).result,
             FileResult::TheStartupEntryIsAlreadyThere);
}

void StartupOnRealDiskTest::RemovingAPathThatIsAlreadyKeptListsTheNewestAndKeepsTheOlderUnderIt()
{
    const TempFiles files;
    ExeXmlStartupEntries startup(StartupFileIn(files, "simulator-exe.xml"));
    startup.KeepRemovedEntriesIn(RemovedFileIn(files));

    StartupBackup backup;

    QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(kIFly)}, backup).result, FileResult::Completed);
    QCOMPARE(startup.Apply(StartupAddition{.label = "iFly again", .path = PathFromUtf8(kIFly)}, backup).result,
             FileResult::Completed);
    QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(kIFly)}, backup).result, FileResult::Completed);

    const std::vector<StartupRemovedEntry> removed = startup.Removed();

    QCOMPARE(removed.size(), std::size_t{1});
    QCOMPARE(removed.front().entry.label, std::string("iFly again"));
    QCOMPARE(RemovedStartupEntriesFile(RemovedFileIn(files)).Entries().size(), std::size_t{2});

    StartupBackup other;

    QCOMPARE(startup.Apply(StartupForgetting{.path = PathFromUtf8(kIFly)}, other).result, FileResult::Completed);

    const std::vector<StartupRemovedEntry> afterTheForgetting = startup.Removed();

    QCOMPARE(afterTheForgetting.size(), std::size_t{1});
    QCOMPARE(afterTheForgetting.front().entry.label, std::string("iFly Plugin"));
}

void StartupOnRealDiskTest::ForgettingAnEntryOnlyTakesItOutOfTheRemovedList()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    ExeXmlStartupEntries startup(file);
    startup.KeepRemovedEntriesIn(RemovedFileIn(files));

    StartupBackup backup;

    QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(kIFly)}, backup).result, FileResult::Completed);

    const std::string withoutIt = BytesOf(file);

    StartupBackup other;

    QCOMPARE(startup.Apply(StartupForgetting{.path = PathFromUtf8(kIFly)}, other).result, FileResult::Completed);

    QVERIFY(startup.Removed().empty());
    QVERIFY(!other.taken);
    QCOMPARE(FirstDifference(BytesOf(file), withoutIt), std::string::npos);
    QCOMPARE(startup.Apply(StartupRestoring{.path = PathFromUtf8(kIFly)}, other).result,
             FileResult::TheDiskDisagreesWithTheScan);
    QCOMPARE(FirstDifference(BytesOf(file), withoutIt), std::string::npos);
}

void StartupOnRealDiskTest::TheFileIsCreatedBesideAUserCfgAndOnlyThen()
{
    const TempFiles files;
    static_cast<void>(files.WriteText("UserCfg.opt", "{ }"));
    const std::filesystem::path file = files.Root() / "EXE.xml";
    ExeXmlStartupEntries startup(file);

    StartupBackup backup;
    const StartupApplied applied = startup.Apply(
        StartupAddition{.label = "Tool", .path = PathFromUtf8(R"(C:\Tools\tool.exe)"), .commandLine = "--go"}, backup);

    const std::string expected = "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                 "<SimBase.Document Type=\"Launch\" version=\"1,0\">\r\n"
                                 "  <Descr>Launch</Descr>\r\n"
                                 "  <Filename>EXE.xml</Filename>\r\n"
                                 "  <Disabled>False</Disabled>\r\n"
                                 "  <Launch.ManualLoad>False</Launch.ManualLoad>\r\n"
                                 "  <Launch.Addon>\r\n"
                                 "    <Name>Tool</Name>\r\n"
                                 "    <Disabled>False</Disabled>\r\n"
                                 "    <Path>C:\\Tools\\tool.exe</Path>\r\n"
                                 "    <CommandLine>--go</CommandLine>\r\n"
                                 "  </Launch.Addon>\r\n"
                                 "</SimBase.Document>\r\n";

    QCOMPARE(applied.result, FileResult::Completed);
    QCOMPARE(FirstDifference(BytesOf(file), expected), std::string::npos);
    QVERIFY(!backup.taken);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{2});
    QCOMPARE(startup.Entries().size(), std::size_t{1});

    QCOMPARE(startup.Apply(StartupAddition{.label = "Second", .path = PathFromUtf8(R"(C:\Tools\second.exe)")}, backup)
                 .result,
             FileResult::Completed);
    QCOMPARE(startup.Entries().size(), std::size_t{2});
    QVERIFY(backup.taken);
}

void StartupOnRealDiskTest::NothingIsCreatedWhereThereIsNoUserCfgBesideTheFile()
{
    const TempFiles files;
    ExeXmlStartupEntries startup(files.Root() / "EXE.xml");

    StartupBackup backup;
    const StartupAddition addition{.label = "Tool", .path = PathFromUtf8(R"(C:\Tools\tool.exe)")};

    QCOMPARE(startup.Apply(addition, backup).result, FileResult::CouldNotReadTheStartupFile);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{0});

    ExeXmlStartupEntries inAFolderThatIsNotThere(files.Root() / "missing" / "EXE.xml");

    QCOMPARE(inAFolderThatIsNotThere.Apply(addition, backup).result, FileResult::CouldNotReadTheStartupFile);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{0});
}

void StartupOnRealDiskTest::OnlyAnAdditionCreatesTheFileEvenBesideAUserCfg()
{
    const TempFiles files;
    static_cast<void>(files.WriteText("UserCfg.opt", "{ }"));
    ExeXmlStartupEntries startup(files.Root() / "EXE.xml");
    startup.KeepRemovedEntriesIn(RemovedFileIn(files));

    StartupBackup backup;

    QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(kIFly)}, backup).result,
             FileResult::CouldNotReadTheStartupFile);
    QCOMPARE(
        startup.Apply(StartupEditing{.path = PathFromUtf8(kIFly), .label = "x", .newPath = PathFromUtf8(kIFly)}, backup)
            .result,
        FileResult::CouldNotReadTheStartupFile);
    QCOMPARE(startup.Apply(StartupRestoring{.path = PathFromUtf8(kIFly)}, backup).result,
             FileResult::CouldNotReadTheStartupFile);
    QCOMPARE(startup.Switch(PathFromUtf8(kIFly), false), FileResult::CouldNotReadTheStartupFile);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{1});
}

void StartupOnRealDiskTest::ABatchOfNewOperationsLeavesTheBackupAsItWasBeforeTheFirstWrite()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    ExeXmlStartupEntries startup(file);
    startup.KeepRemovedEntriesIn(RemovedFileIn(files));

    StartupBackup batch;

    QCOMPARE(
        startup.Apply(StartupAddition{.label = "Tool", .path = PathFromUtf8(R"(C:\Tools\tool.exe)")}, batch).result,
        FileResult::Completed);
    QVERIFY(batch.taken);
    QCOMPARE(FirstDifference(BytesOf(BackupOfStartupFile(file)), Fixture("simulator-exe.xml")), std::string::npos);

    QCOMPARE(startup
                 .Apply(StartupEditing{.path = PathFromUtf8(kFenix), .label = "Fenix", .newPath = PathFromUtf8(kFenix)},
                        batch)
                 .result,
             FileResult::Completed);
    QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(kIFly)}, batch).result, FileResult::Completed);
    QCOMPARE(startup.Apply(StartupRestoring{.path = PathFromUtf8(kIFly)}, batch).result, FileResult::Completed);

    QCOMPARE(FirstDifference(BytesOf(BackupOfStartupFile(file)), Fixture("simulator-exe.xml")), std::string::npos);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{3});

    StartupBackup alone;

    QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(kFsRealistic)}, alone).result, FileResult::Completed);
    QVERIFY(alone.taken);
    QCOMPARE(ExeXmlStartupEntries(BackupOfStartupFile(file)).Entries().size(), std::size_t{22});
}

void StartupOnRealDiskTest::ARefusedChangeLeavesNoBackupAndNoFileBehind()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    ExeXmlStartupEntries startup(file);
    startup.KeepRemovedEntriesIn(RemovedFileIn(files));

    StartupBackup backup;

    QCOMPARE(startup.Apply(StartupAddition{.label = "Again", .path = PathFromUtf8(kIFly)}, backup).result,
             FileResult::TheStartupEntryIsAlreadyThere);
    QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(R"(C:\Nothing\here.exe)")}, backup).result,
             FileResult::TheDiskDisagreesWithTheScan);
    QCOMPARE(
        startup
            .Apply(StartupEditing{.path = PathFromUtf8(kIFly), .label = "x", .newPath = PathFromUtf8(kFsRealistic)},
                   backup)
            .result,
        FileResult::TheStartupEntryIsAlreadyThere);

    QCOMPARE(FirstDifference(BytesOf(file), Fixture("simulator-exe.xml")), std::string::npos);
    QVERIFY(!backup.taken);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{1});
}

void StartupOnRealDiskTest::AnEditThatMovesTheEntryKeepsTheOldBlockAndOneThatDoesNotKeepsNothing()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    ExeXmlStartupEntries startup(file);
    startup.KeepRemovedEntriesIn(RemovedFileIn(files));

    StartupBackup backup;

    const StartupApplied relabelled = startup.Apply(
        StartupEditing{
            .path = PathFromUtf8(kIFly), .label = "iFly 737", .newPath = PathFromUtf8(kIFly), .commandLine = "auto"},
        backup);

    QCOMPARE(relabelled.result, FileResult::Completed);
    QVERIFY(relabelled.was.has_value());
    QCOMPARE(relabelled.was->label, std::string("iFly Plugin"));
    QVERIFY(startup.Removed().empty());
    QVERIFY(!std::filesystem::exists(RemovedFileIn(files)));

    const StartupApplied moved = startup.Apply(StartupEditing{.path = PathFromUtf8(kIFly),
                                                              .label = "iFly 737",
                                                              .newPath = PathFromUtf8(R"(E:\Elsewhere\iFly.exe)"),
                                                              .commandLine = ""},
                                               backup);

    QCOMPARE(moved.result, FileResult::Completed);

    const std::vector<StartupRemovedEntry> removed = startup.Removed();

    QCOMPARE(removed.size(), std::size_t{1});
    QCOMPARE(removed.front().entry.path, PathFromUtf8(kIFly));
    QCOMPARE(removed.front().entry.label, std::string("iFly 737"));
    QCOMPARE(removed.front().entry.commandLine, std::string("auto"));

    QCOMPARE(startup.Apply(StartupRestoring{.path = PathFromUtf8(kIFly)}, backup).result, FileResult::Completed);
    QCOMPARE(startup.Entries().size(), std::size_t{22});
    QVERIFY(startup.Removed().empty());
}

void StartupOnRealDiskTest::AWindows1252FileKeepsItsBytesThroughARemovalAndARestore()
{
    const TempFiles files;
    const std::filesystem::path file = files.WriteText("EXE.xml", std::string(kAccentedFileIn1252));
    ExeXmlStartupEntries startup(file);
    startup.KeepRemovedEntriesIn(RemovedFileIn(files));

    QCOMPARE(startup.Entries().size(), std::size_t{2});

    StartupBackup backup;
    const std::filesystem::path accented = PathFromUtf8("C:\\Avi\xC3\xB5"
                                                        "es\\x.exe");

    QCOMPARE(startup.Apply(StartupRemoval{.path = accented}, backup).result, FileResult::Completed);
    QCOMPARE(startup.Entries().size(), std::size_t{1});
    QCOMPARE(startup.Removed().front().entry.label,
             std::string("Avi\xC3\xB5"
                         "es"));
    QCOMPARE(startup.Apply(StartupRestoring{.path = accented}, backup).result, FileResult::Completed);
    QCOMPARE(FirstDifference(BytesOf(file), std::string(kAccentedFileIn1252)), std::string::npos);
}

void StartupOnRealDiskTest::AnAdditionTheFileCannotHoldWritesNothing()
{
    const TempFiles files;
    const std::filesystem::path file = files.WriteText("EXE.xml", std::string(kAccentedFileIn1252));
    ExeXmlStartupEntries startup(file);

    StartupBackup backup;

    QCOMPARE(startup.Apply(StartupAddition{.label = "Ol\xC3\xA1", .path = PathFromUtf8(R"(C:\Tools\tool.exe)")}, backup)
                 .result,
             FileResult::TheStartupFileIsNotUtf8);
    QCOMPARE(BytesOf(file), std::string(kAccentedFileIn1252));
    QVERIFY(!backup.taken);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{1});
}

void StartupOnRealDiskTest::AnEmptyFilePathNeverLooksForUserCfgInTheCurrentFolder()
{
    const TempFiles files;
    static_cast<void>(files.WriteText("UserCfg.opt", "{ }"));

    std::error_code error;
    const std::filesystem::path before = std::filesystem::current_path(error);
    std::filesystem::current_path(files.Root(), error);
    QVERIFY(!error);

    ExeXmlStartupEntries startup{{}};
    StartupBackup backup;
    const StartupApplied applied = startup.Apply(
        StartupAddition{.label = "Tool", .path = PathFromUtf8(R"(C:\Tools\tool.exe)"), .commandLine = "--go"}, backup);

    std::filesystem::current_path(before, error);

    QCOMPARE(applied.result, FileResult::CouldNotReadTheStartupFile);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{1});
    QVERIFY(!backup.taken);
}

void StartupOnRealDiskTest::AUtf16FileIsReadButEveryWriteIsRefusedAndNothingIsLeftBehind()
{
    for (const bool bigEndian : {false, true})
    {
        const TempFiles files;
        const std::string bytes = Utf16WithByteOrderMarkOf(kUtf16Document, bigEndian);
        const std::filesystem::path file = files.WriteText("EXE.xml", bytes);
        const std::filesystem::path accented = PathFromUtf8("C:\\Avi\xC3\xB5"
                                                            "es\\x.exe");

        ExeXmlStartupEntries startup(file);
        startup.KeepRemovedEntriesIn(RemovedFileIn(files));

        const std::vector<StartupEntry> read = startup.Entries();

        QCOMPARE(read.size(), std::size_t{1});
        QCOMPARE(read.front().path, accented);
        QVERIFY(read.front().enabled);

        StartupBackup backup;
        const std::vector<StartupChange> changes{
            StartupSwitching{.path = accented, .enabled = false},
            StartupAddition{.label = "Tool", .path = PathFromUtf8(R"(C:\Tools\tool.exe)")},
            StartupRemoval{.path = accented}, StartupEditing{.path = accented, .label = "Renamed", .newPath = accented},
            StartupRestoring{.path = accented}};

        for (const StartupChange& change : changes)
        {
            QCOMPARE(startup.Apply(change, backup).result, FileResult::TheStartupFileIsNotUtf8);
        }

        QCOMPARE(BytesOf(file), bytes);
        QVERIFY(!backup.taken);
        QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{1});
    }
}

void StartupOnRealDiskTest::TheRemovedFileIsNamedByTheSimulatorVariantAndCreatesItsFolder()
{
    const TempFiles files;
    const std::filesystem::path folder = files.Root() / "removed" / "entries";

    QCOMPARE(RemovedStartupEntriesFileOf(folder, SimulatorVariant::MSFS2024), folder / "msfs2024.json");
    QCOMPARE(RemovedStartupEntriesFileOf(folder, SimulatorVariant::MSFS2020), folder / "msfs2020.json");

    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    ExeXmlStartupEntries startup(file);
    startup.KeepRemovedEntriesIn(RemovedStartupEntriesFileOf(folder, SimulatorVariant::MSFS2024));

    StartupBackup backup;

    QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(kIFly)}, backup).result, FileResult::Completed);
    QVERIFY(std::filesystem::exists(folder / "msfs2024.json"));
    QCOMPARE(startup.Removed().size(), std::size_t{1});

    ExeXmlStartupEntries ofAnotherProfileOnTheSameFile(file);
    ofAnotherProfileOnTheSameFile.KeepRemovedEntriesIn(RemovedStartupEntriesFileOf(folder, SimulatorVariant::MSFS2024));

    QCOMPARE(ofAnotherProfileOnTheSameFile.Removed().size(), std::size_t{1});

    ExeXmlStartupEntries nowhere(file);
    nowhere.KeepRemovedEntriesIn({});

    StartupBackup other;

    QCOMPARE(nowhere.Apply(StartupRemoval{.path = PathFromUtf8(kFslControlCenter)}, other).result,
             FileResult::CouldNotKeepTheRemovedEntry);
    QVERIFY(!other.taken);
}

void StartupOnRealDiskTest::ARemovedEntryIsStampedWithTheInstantOfTheEditThatMovedIt()
{
    const TempFiles files;
    ExeXmlStartupEntries startup(StartupFileIn(files, "simulator-exe.xml"));
    startup.KeepRemovedEntriesIn(RemovedFileIn(files));

    StartupBackup backup;

    QCOMPARE(startup
                 .Apply(StartupEditing{.path = PathFromUtf8(kIFly),
                                       .label = "iFly Plugin",
                                       .newPath = PathFromUtf8(R"(E:\Elsewhere\iFly.exe)"),
                                       .commandLine = "auto",
                                       .at = Moment(777'000)},
                        backup)
                 .result,
             FileResult::Completed);

    const std::vector<StartupRemovedEntry> removed = startup.Removed();

    QCOMPARE(removed.size(), std::size_t{1});
    QCOMPARE(removed.front().removedAt, Moment(777'000));
}

void StartupOnRealDiskTest::RemovalsWithTheSameInstantListTheLastRemovedFirst()
{
    const TempFiles files;
    ExeXmlStartupEntries startup(StartupFileIn(files, "simulator-exe.xml"));
    startup.KeepRemovedEntriesIn(RemovedFileIn(files));

    StartupBackup backup;

    QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(kIFly), .at = Moment(5'000)}, backup).result,
             FileResult::Completed);
    QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(kFslControlCenter), .at = Moment(5'000)}, backup).result,
             FileResult::Completed);
    QCOMPARE(startup.Apply(StartupRemoval{.path = PathFromUtf8(kDynamicLod)}, backup).result, FileResult::Completed);

    const std::vector<StartupRemovedEntry> removed = startup.Removed();

    QCOMPARE(removed.size(), std::size_t{3});
    QCOMPARE(removed[0].entry.path, PathFromUtf8(kFslControlCenter));
    QCOMPARE(removed[1].entry.path, PathFromUtf8(kIFly));
    QCOMPARE(removed[2].entry.path, PathFromUtf8(kDynamicLod));
}

void StartupOnRealDiskTest::AFileThatExistsButCannotBeReadIsNeverStartedOverByAnAddition()
{
    const TempFiles files;
    static_cast<void>(files.WriteText("UserCfg.opt", "{ }"));
    const std::filesystem::path file = files.Root() / "EXE.xml";
    std::filesystem::create_directories(file);
    ExeXmlStartupEntries startup(file);

    StartupBackup backup;
    const StartupApplied applied =
        startup.Apply(StartupAddition{.label = "Tool", .path = PathFromUtf8(R"(C:\Tools\tool.exe)")}, backup);

    QCOMPARE(applied.result, FileResult::CouldNotReadTheStartupFile);
    QVERIFY(std::filesystem::is_directory(file));
    QVERIFY(!backup.taken);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{2});
}

void StartupOnRealDiskTest::TwoChangesAtTheSameTimeBothLandInTheFile()
{
    const TempFiles files;
    ExeXmlStartupEntries startup(StartupFileIn(files, "simulator-exe.xml"));

    constexpr int kRounds = 25;
    std::size_t refused = 0;
    std::size_t lost = 0;

    for (int round = 0; round < kRounds; ++round)
    {
        const bool any2GsxOn = round % 2 == 0;
        const bool fsRealisticOn = round % 2 == 1;
        std::atomic<bool> go = false;
        std::atomic<std::size_t> refusals = 0;

        const auto switching = [&](const char* path, const bool enabled)
        {
            while (!go)
            {
                std::this_thread::yield();
            }

            if (startup.Switch(PathFromUtf8(path), enabled) != FileResult::Completed)
            {
                ++refusals;
            }
        };

        std::thread first(switching, kAny2GSX, any2GsxOn);
        std::thread second(switching, kFsRealistic, fsRealisticOn);
        go = true;
        first.join();
        second.join();

        refused += refusals;

        const std::vector<StartupEntry> entries = startup.Entries();

        if (EnabledIn(entries, "Any2GSX") != any2GsxOn || EnabledIn(entries, "FSRealistic+") != fsRealisticOn)
        {
            ++lost;
        }
    }

    QCOMPARE(refused, std::size_t{0});
    QCOMPARE(lost, std::size_t{0});
}

void StartupOnRealDiskTest::AChangeTheLocatorAndTheReaderDisagreeAboutIsRefusedAndWritesNothing()
{
    const TempFiles files;
    const std::string document = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                 "<SimBase.Document Type=\"Launch\" version=\"1,0\">\r\n"
                                 "  <![CDATA[\r\n"
                                 "  <Launch.Addon>\r\n"
                                 "    <Name>Ghost</Name>\r\n"
                                 "    <Path>C:\\Ghost\\ghost.exe</Path>\r\n"
                                 "  </Launch.Addon>\r\n"
                                 "  ]]>\r\n"
                                 "  <Launch.Addon>\r\n"
                                 "    <Name>Real</Name>\r\n"
                                 "    <Path>C:\\Real\\real.exe</Path>\r\n"
                                 "  </Launch.Addon>\r\n"
                                 "</SimBase.Document>\r\n";
    const std::filesystem::path file = files.WriteText("EXE.xml", document);
    ExeXmlStartupEntries startup(file);
    startup.KeepRemovedEntriesIn(RemovedFileIn(files));

    QCOMPARE(startup.Entries().size(), std::size_t{1});

    StartupBackup backup;
    const StartupApplied applied = startup.Apply(StartupRemoval{.path = PathFromUtf8(R"(C:\Ghost\ghost.exe)")}, backup);

    QCOMPARE(applied.result, FileResult::CouldNotWriteTheStartupFile);
    QCOMPARE(BytesOf(file), document);
    QVERIFY(!backup.taken);
    QVERIFY(!std::filesystem::exists(BackupOfStartupFile(file)));
    QVERIFY(startup.Removed().empty());
}

void StartupOnRealDiskTest::ANamelessEntryNamedAndEmptiedThroughTheAdapterLeavesWellFormedXml()
{
    const TempFiles files;
    const std::filesystem::path path = PathFromUtf8(R"(C:\First\tool.exe)");
    const std::filesystem::path file = files.WriteText("EXE.xml",
                                                       "<SimBase.Document Type=\"Launch\" version=\"1,0\">\n"
                                                       "    <Launch.Addon>\n"
                                                       "        <CommandLine>-x</CommandLine>\n"
                                                       "        <Disabled>True</Disabled>\n"
                                                       "        <Path>C:\\First\\tool.exe</Path>\n"
                                                       "    </Launch.Addon>\n"
                                                       "</SimBase.Document>\n");
    ExeXmlStartupEntries startup(file);

    StartupBackup backup;
    const StartupApplied applied =
        startup.Apply(StartupEditing{.path = path, .label = "Tool", .newPath = path, .commandLine = ""}, backup);

    QCOMPARE(applied.result, FileResult::Completed);

    const std::vector<StartupEntry> entries = startup.Entries();

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().label, std::string("Tool"));
    QCOMPARE(entries.front().path, path);
    QVERIFY(entries.front().commandLine.empty());
    QVERIFY(!entries.front().enabled);
}

void StartupOnRealDiskTest::AnEditToWhatTheEntryHoldsSaysNothingChangedAndWritesNothing()
{
    const TempFiles files;
    const std::filesystem::path file = StartupFileIn(files, "simulator-exe.xml");
    ExeXmlStartupEntries startup(file);

    StartupBackup backup;
    const StartupApplied same = startup.Apply(
        StartupEditing{
            .path = PathFromUtf8(kIFly), .label = "iFly Plugin", .newPath = PathFromUtf8(kIFly), .commandLine = "auto"},
        backup);

    QCOMPARE(same.result, FileResult::Completed);
    QVERIFY(same.changedNothing);
    QVERIFY(same.was.has_value());
    QVERIFY(!backup.taken);
    QCOMPARE(FirstDifference(BytesOf(file), Fixture("simulator-exe.xml")), std::string::npos);

    const StartupApplied different = startup.Apply(StartupEditing{.path = PathFromUtf8(kIFly),
                                                                  .label = "iFly Plugin",
                                                                  .newPath = PathFromUtf8(kIFly),
                                                                  .commandLine = "manual"},
                                                   backup);

    QCOMPARE(different.result, FileResult::Completed);
    QVERIFY(!different.changedNothing);
    QVERIFY(backup.taken);
}

QTEST_APPLESS_MAIN(StartupOnRealDiskTest)

#include "tst_startup_on_real_disk.moc"
