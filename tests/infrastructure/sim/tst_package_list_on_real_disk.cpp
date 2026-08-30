#include <QtTest/QtTest>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "application/ports/PackageList.h"
#include "infrastructure/sim/ContentXmlPackageList.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/TempFiles.h"

namespace
{
    class PackageListOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheEntriesComeBackFromTheFileOnDisk();
        static void TheSwitchLandsOnDiskAndTheNextReadSeesIt();
        static void TheBackupIsTheVersionImmediatelyBeforeTheWrite();
        static void NothingIsWrittenWhenTheBackupCannotBeMade();
        static void AFileThatIsNotThereIsRefusedBeforeAnythingIsWritten();
        static void APackageTheFileNoLongerCarriesIsRefusedAndNoBackupIsLeft();
        static void SwitchingAnEntryToTheValueItAlreadyHasWritesNothing();
        static void TheFileIsRereadAtTheInstantOfWriting();
        static void PointingItAtAnotherProfilesFileReadsAndWritesThatOne();
        static void ABatchOfSwitchesWritesTheFileOnceAndTheBackupIsTheVersionBeforeTheBatch();
    };

    constexpr auto kLpma = "fs24-asobo-airport-lpma-madeira";
    constexpr auto kAnimals = "communityfs24-aaa-simaddons-animals";

    [[nodiscard]] std::string BytesOf(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);

        return std::string(std::istreambuf_iterator(stream), std::istreambuf_iterator<char>());
    }

    [[nodiscard]] std::string Fixture(const std::string& name)
    {
        return BytesOf(std::filesystem::path(FSORG_FIXTURES_DIR) / name);
    }

    [[nodiscard]] std::filesystem::path ListIn(const TempFiles& files, const std::string& fixture)
    {
        const std::filesystem::path file = files.Root() / "Content.xml";
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

    [[nodiscard]] PackageActivation ActivationOf(const std::vector<PackageEntry>& entries, const std::string& name)
    {
        const auto found = std::ranges::find(entries, name, &PackageEntry::name);

        return found == entries.end() ? PackageActivation::ItSaysSomethingElse : found->activation;
    }
}

void PackageListOnRealDiskTest::TheEntriesComeBackFromTheFileOnDisk()
{
    const TempFiles files;
    const ContentXmlPackageList list(ListIn(files, "simulator-content.xml"));

    const std::vector<PackageEntry> entries = list.Entries();

    QCOMPARE(entries.size(), std::size_t{9});
    QCOMPARE(ActivationOf(entries, kLpma), PackageActivation::UserDisabled);
    QCOMPARE(ActivationOf(entries, kAnimals), PackageActivation::Activated);
}

void PackageListOnRealDiskTest::TheSwitchLandsOnDiskAndTheNextReadSeesIt()
{
    const TempFiles files;
    const std::filesystem::path file = ListIn(files, "simulator-content.xml");
    ContentXmlPackageList list(file);

    QCOMPARE(list.Switch(kLpma, true), FileResult::Completed);

    QCOMPARE(FirstDifference(BytesOf(file), Fixture("simulator-content-lpma-activated.xml")), std::string::npos);
    QCOMPARE(ActivationOf(list.Entries(), kLpma), PackageActivation::Activated);
}

void PackageListOnRealDiskTest::TheBackupIsTheVersionImmediatelyBeforeTheWrite()
{
    const TempFiles files;
    const std::filesystem::path file = ListIn(files, "simulator-content.xml");
    const std::filesystem::path backup = BackupOfPackageList(file);
    ContentXmlPackageList list(file);

    QCOMPARE(list.Switch(kLpma, true), FileResult::Completed);
    QCOMPARE(FirstDifference(BytesOf(backup), Fixture("simulator-content.xml")), std::string::npos);

    QCOMPARE(list.Switch(kAnimals, false), FileResult::Completed);
    QCOMPARE(FirstDifference(BytesOf(backup), Fixture("simulator-content-lpma-activated.xml")), std::string::npos);
    QVERIFY2(HowManyFilesIn(files.Root()) == std::size_t{2},
             "the copy is one file, overwritten and never dated, in the mould of the startup file");
}

void PackageListOnRealDiskTest::NothingIsWrittenWhenTheBackupCannotBeMade()
{
    const TempFiles files;
    const std::filesystem::path file = ListIn(files, "simulator-content.xml");
    std::filesystem::create_directories(BackupOfPackageList(file));
    ContentXmlPackageList list(file);

    QCOMPARE(list.Switch(kLpma, true), FileResult::CouldNotWriteThePackageList);
    QVERIFY2(FirstDifference(BytesOf(file), Fixture("simulator-content.xml")) == std::string::npos,
             "bad XML makes the simulator wipe the whole list, so the copy is what has to exist before the write");
}

void PackageListOnRealDiskTest::AFileThatIsNotThereIsRefusedBeforeAnythingIsWritten()
{
    const TempFiles files;
    ContentXmlPackageList list(files.Root() / "Content.xml");

    QVERIFY(list.Entries().empty());
    QCOMPARE(list.Switch(kLpma, true), FileResult::CouldNotReadThePackageList);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{0});
}

void PackageListOnRealDiskTest::APackageTheFileNoLongerCarriesIsRefusedAndNoBackupIsLeft()
{
    const TempFiles files;
    const std::filesystem::path file = ListIn(files, "simulator-content.xml");
    ContentXmlPackageList list(file);

    QCOMPARE(list.Switch("fs24-nobody-ever-installed-this", true), FileResult::TheDiskDisagreesWithTheScan);
    QCOMPARE(FirstDifference(BytesOf(file), Fixture("simulator-content.xml")), std::string::npos);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{1});
}

void PackageListOnRealDiskTest::SwitchingAnEntryToTheValueItAlreadyHasWritesNothing()
{
    const TempFiles files;
    const std::filesystem::path file = ListIn(files, "simulator-content.xml");
    ContentXmlPackageList list(file);

    QCOMPARE(list.Switch(kAnimals, true), FileResult::Completed);
    QCOMPARE(FirstDifference(BytesOf(file), Fixture("simulator-content.xml")), std::string::npos);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{1});
}

void PackageListOnRealDiskTest::TheFileIsRereadAtTheInstantOfWriting()
{
    const TempFiles files;
    const std::filesystem::path file = ListIn(files, "simulator-content.xml");
    ContentXmlPackageList list(file);

    std::ofstream(file, std::ios::binary | std::ios::trunc) << Fixture("simulator-content-lpma-activated.xml");

    QCOMPARE(list.Switch(kAnimals, false), FileResult::Completed);
    QVERIFY2(ActivationOf(list.Entries(), kLpma) == PackageActivation::Activated,
             "the simulator rewrites this file whenever it runs, so what is written has to be spliced into what the "
             "file says now and not into what the app read earlier");
}

void PackageListOnRealDiskTest::PointingItAtAnotherProfilesFileReadsAndWritesThatOne()
{
    const TempFiles files;
    const std::filesystem::path other = files.Root() / "other";
    std::filesystem::create_directories(other);
    std::ofstream(other / "Content.xml", std::ios::binary) << Fixture("simulator-content-lpma-activated.xml");

    ContentXmlPackageList list(files.Root() / "Content.xml");
    QVERIFY(list.Entries().empty());

    list.Use(other / "Content.xml");

    QCOMPARE(list.Switch(kLpma, false), FileResult::Completed);
    QCOMPARE(FirstDifference(BytesOf(other / "Content.xml"), Fixture("simulator-content.xml")), std::string::npos);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{1});
}

void PackageListOnRealDiskTest::ABatchOfSwitchesWritesTheFileOnceAndTheBackupIsTheVersionBeforeTheBatch()
{
    const TempFiles files;
    const std::filesystem::path file = ListIn(files, "simulator-content-lpma-activated.xml");
    const std::filesystem::path backup = BackupOfPackageList(file);
    ContentXmlPackageList list(file);

    QCOMPARE(list.SwitchAll({kLpma, kAnimals}, false), FileResult::Completed);

    QVERIFY2(FirstDifference(BytesOf(backup), Fixture("simulator-content-lpma-activated.xml")) == std::string::npos,
             "the copy holds the file as it stood before the whole batch, not before its last package, which is what "
             "one write instead of one per package leaves behind");
    QCOMPARE(ActivationOf(list.Entries(), kLpma), PackageActivation::UserDisabled);
    QCOMPARE(ActivationOf(list.Entries(), kAnimals), PackageActivation::UserDisabled);
    QCOMPARE(HowManyFilesIn(files.Root()), std::size_t{2});
}

QTEST_APPLESS_MAIN(PackageListOnRealDiskTest)

#include "tst_package_list_on_real_disk.moc"
