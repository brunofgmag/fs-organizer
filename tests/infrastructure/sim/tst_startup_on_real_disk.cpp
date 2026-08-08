#include <QtTest/QtTest>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "domain/support/PathUtils.h"
#include "infrastructure/sim/ExeXmlStartupEntries.h"
#include "infrastructure/sim/StartupFileLocations.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
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
    };

    constexpr auto kAny2GSX = R"(C:\Users\pilot\AppData\Roaming\Any2GSX\bin\Any2GSX.exe)";
    constexpr auto kFsRealistic =
        R"(E:\Flight Simulator 2024\Community\rkapps-fsrealistic\service\FSRealistic-Plus.exe)";

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

QTEST_APPLESS_MAIN(StartupOnRealDiskTest)

#include "tst_startup_on_real_disk.moc"
