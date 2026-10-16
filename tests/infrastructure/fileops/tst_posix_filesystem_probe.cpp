#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "domain/importing/VolumeSharing.h"
#include "infrastructure/fileops/MountPoints.h"
#include "infrastructure/fileops/PosixFilesystemProbe.h"
#include "support/FileClock.h"
#include "tests/support/StdFilesystemProbe.h"
#include "support/PathText.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class PosixFilesystemProbeTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ADanglingLinkIsStillAnEntryThatOccupiesItsPath();
        static void ALinkIsAReparsePointAndAFolderOrAFileIsNot();
        static void OnlyARealFolderIsPhysicalAndALinkOverItIsNot();
        static void TargetDirectoryExistsFollowsLinksAndChainsOfLinks();
        static void AFolderAndAFileAreListedEachUnderItsOwnKind();
        static void ALinkToAFolderIsListedAsAFolderAndADanglingOneStaysAmongThem();
        static void TheStandardLibraryDoubleAnswersALinkTheSameWayThisProbeDoes();
        static void ANameWithAccentsAndOneThatIsNotUtf8ComeBackIntact();
        static void AFolderThatIsNotThereOrHasNothingInItListsNothing();
        static void AnAbsolutePathIsOnAnAvailableVolumeAndARelativeOneIsNot();
        static void TwoPathsInTheSameFolderShareAVolumeAndAPathNotThereYetFollowsItsParent();
        static void APathOnAnotherFilesystemSharesNoVolumeWithTheDisk();
        static void ABindMountOfTheSameFilesystemIsAnotherVolume();
        static void APathThatIsRelativeOrEmptyHasNoVolumeToShare();
        static void AMountThatIsConfiguredButNotMountedMakesTheVolumeUnavailable();
        static void EveryMountOfThisMachineIsAnAvailableVolume();
        static void AFolderThatIsNotThereIsNotTheSameAsOneThatRefusesTheWrite();
        static void AFolderThatTakesTheWriteLeavesNoProbeBehind();
        static void AFolderTheProcessCannotWriteSaysPermissionIsWhatStoppedIt();
        static void AProbeNameTakenByAFolderIsARefusalForAnotherReason();
        static void NothingHoldsAFileOpenBecauseUnlinkingAnOpenFileWorksOnLinux();
        static void FreeSpaceIsOnlyAnswerableForAFolderThatAlreadyExists();
        static void TheTrashReportsNoQuotaAndNoPathCeiling();
        static void ContentsAndFirstBytesComeBackByteForByte();
        static void AFolderIsNotReadAsAFile();
        static void TheHashIsTheSha256OfTheContents();
        static void ManyHashesComeBackEachInItsPlaceAndAMissingFileAnswersNothing();
        static void TheFingerprintNamesEachFileRelativeToTheRootWhetherOrNotTheRootEndsInASeparator();
        static void TheFingerprintCarriesTheWriteTimeOfEachFileAsTheFileSystemHoldsIt();
        static void TheLongestEntryOfAFingerprintIsTheLongestPathUnderTheRoot();
        static void TheFingerprintOfARootThatIsNotThereAnswersNothing();
        static void AFolderReportsWhenItWasLastWrittenTo();
    };
}

namespace
{
    struct Disk
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return AsPath(directory.path());
        }

        [[nodiscard]] std::filesystem::path AddFolder(const std::string& relativePath) const
        {
            const std::filesystem::path folder = Root() / relativePath;
            std::filesystem::create_directories(folder);

            return folder;
        }

        [[nodiscard]] std::filesystem::path AddFile(const std::string& relativePath, const std::string& bytes) const
        {
            const std::filesystem::path file = Root() / relativePath;
            std::filesystem::create_directories(file.parent_path());

            std::ofstream stream(file, std::ios::binary);
            stream << bytes;

            return file;
        }

        [[nodiscard]] std::filesystem::path AddLink(const std::string& relativePath,
                                                    const std::filesystem::path& target) const
        {
            const std::filesystem::path link = Root() / relativePath;
            std::filesystem::create_directory_symlink(target, link);

            return link;
        }
    };

    class RemovedAtTheEnd
    {
    public:
        explicit RemovedAtTheEnd(std::filesystem::path path) : path_(std::move(path))
        {
        }

        RemovedAtTheEnd(const RemovedAtTheEnd&) = delete;
        RemovedAtTheEnd& operator=(const RemovedAtTheEnd&) = delete;

        ~RemovedAtTheEnd()
        {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }

    private:
        std::filesystem::path path_;
    };

    dev_t DeviceOf(const std::filesystem::path& path)
    {
        struct stat status
        {
        };

        return ::stat(path.c_str(), &status) == 0 ? status.st_dev : 0;
    }

    std::filesystem::path AFolderOnAnotherFilesystem(const std::filesystem::path& besides)
    {
        for (const char* candidate : {"/dev/shm", "/run"})
        {
            std::error_code error;
            std::filesystem::path folder = candidate;
            std::filesystem::path mine = folder / ("fsorg-volume-" + std::to_string(::getpid()));
            if (DeviceOf(folder) == 0 || DeviceOf(folder) == DeviceOf(besides)
                || !std::filesystem::create_directories(mine, error))
            {
                continue;
            }

            return mine;
        }

        return {};
    }

    std::vector<std::string> NamesOf(const std::vector<std::filesystem::path>& paths)
    {
        std::vector<std::string> names;
        for (const std::filesystem::path& path : paths)
        {
            names.push_back(path.filename().string());
        }

        return names;
    }
}

void PosixFilesystemProbeTest::ADanglingLinkIsStillAnEntryThatOccupiesItsPath()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("library/gone");
    const std::filesystem::path link = disk.AddLink("dangling", folder);
    std::filesystem::remove_all(folder);

    const PosixFilesystemProbe probe;

    QVERIFY(probe.EntryExistsWithoutFollowingLinks(link));
    QVERIFY(!probe.TargetDirectoryExists(link));
    QVERIFY(!probe.EntryExistsWithoutFollowingLinks(disk.Root() / "never-there"));
    QVERIFY(probe.EntryExistsWithoutFollowingLinks(disk.Root()));
}

void PosixFilesystemProbeTest::ALinkIsAReparsePointAndAFolderOrAFileIsNot()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("library/fenix");
    const std::filesystem::path file = disk.AddFile("library/readme.txt", "x");
    const std::filesystem::path link = disk.AddLink("fenix", folder);

    const PosixFilesystemProbe probe;

    QVERIFY(probe.IsReparsePoint(link));
    QVERIFY(!probe.IsReparsePoint(folder));
    QVERIFY(!probe.IsReparsePoint(file));
    QVERIFY(!probe.IsReparsePoint(disk.Root() / "never-there"));
}

void PosixFilesystemProbeTest::OnlyARealFolderIsPhysicalAndALinkOverItIsNot()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("library/fenix");
    const std::filesystem::path file = disk.AddFile("library/readme.txt", "x");
    const std::filesystem::path link = disk.AddLink("fenix", folder);

    const PosixFilesystemProbe probe;

    QVERIFY(probe.PhysicalDirectoryExists(folder));
    QVERIFY(!probe.PhysicalDirectoryExists(link));
    QVERIFY(!probe.PhysicalDirectoryExists(file));
    QVERIFY(!probe.PhysicalDirectoryExists(disk.Root() / "never-there"));
}

void PosixFilesystemProbeTest::TargetDirectoryExistsFollowsLinksAndChainsOfLinks()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("library/fenix");
    const std::filesystem::path file = disk.AddFile("library/readme.txt", "x");
    const std::filesystem::path first = disk.AddLink("first", folder);
    const std::filesystem::path second = disk.AddLink("second", first);

    const PosixFilesystemProbe probe;

    QVERIFY(probe.TargetDirectoryExists(folder));
    QVERIFY(probe.TargetDirectoryExists(first));
    QVERIFY(probe.TargetDirectoryExists(second));
    QVERIFY(!probe.TargetDirectoryExists(file));
    QVERIFY(!probe.TargetDirectoryExists(disk.Root() / "never-there"));
}

void PosixFilesystemProbeTest::AFolderAndAFileAreListedEachUnderItsOwnKind()
{
    const Disk disk;
    static_cast<void>(disk.AddFolder("holder/Aircrafts"));
    static_cast<void>(disk.AddFile("holder/aircraft.cfg", "x"));
    static_cast<void>(disk.AddFile("holder/Aircrafts/deeper.cfg", "x"));
    static_cast<void>(disk.AddFile("holder/.hidden", "x"));

    const PosixFilesystemProbe probe;
    const std::filesystem::path holder = disk.Root() / "holder";

    QCOMPARE(NamesOf(probe.ChildDirectories(holder)), (std::vector<std::string>{"Aircrafts"}));
    QCOMPARE(NamesOf(probe.ChildFiles(holder)), (std::vector<std::string>{".hidden", "aircraft.cfg"}));
}

void PosixFilesystemProbeTest::ALinkToAFolderIsListedAsAFolderAndADanglingOneStaysAmongThem()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("library/fenix");
    const std::filesystem::path gone = disk.AddFolder("library/gone");
    const std::filesystem::path file = disk.AddFile("library/readme.txt", "x");
    static_cast<void>(disk.AddFolder("Community"));
    static_cast<void>(disk.AddLink("Community/live", folder));
    static_cast<void>(disk.AddLink("Community/dangling", gone));
    static_cast<void>(disk.AddLink("Community/to-a-file", file));
    std::filesystem::remove_all(gone);

    const PosixFilesystemProbe probe;
    const std::filesystem::path community = disk.Root() / "Community";

    QCOMPARE(NamesOf(probe.ChildDirectories(community)), (std::vector<std::string>{"dangling", "live"}));
    QCOMPARE(NamesOf(probe.ChildFiles(community)), (std::vector<std::string>{"to-a-file"}));
}

void PosixFilesystemProbeTest::TheStandardLibraryDoubleAnswersALinkTheSameWayThisProbeDoes()
{
    const Disk disk;
    const std::filesystem::path destination = disk.AddFolder("Community");
    const std::filesystem::path physical = disk.AddFolder("Community/asfs");
    const std::filesystem::path live = disk.AddFolder("live-target");
    const std::filesystem::path liveLink = disk.AddLink("Community/ag-airport-live", live);
    const std::filesystem::path gone = disk.AddFolder("gone-target");
    const std::filesystem::path dangling = disk.AddLink("Community/ag-airport-bgqq", gone);
    std::filesystem::remove_all(gone);

    const PosixFilesystemProbe production;
    const StdFilesystemProbe double_;

    for (const std::filesystem::path& entry : {physical, liveLink, dangling})
    {
        QCOMPARE(double_.IsReparsePoint(entry), production.IsReparsePoint(entry));
        QCOMPARE(double_.EntryExistsWithoutFollowingLinks(entry), production.EntryExistsWithoutFollowingLinks(entry));
    }

    std::vector<std::filesystem::path> byTheDouble = double_.ChildDirectories(destination);
    std::vector<std::filesystem::path> byProduction = production.ChildDirectories(destination);
    std::ranges::sort(byTheDouble);
    std::ranges::sort(byProduction);

    QCOMPARE(byProduction.size(), std::size_t{3});
    QCOMPARE(byTheDouble, byProduction);
}

void PosixFilesystemProbeTest::ANameWithAccentsAndOneThatIsNotUtf8ComeBackIntact()
{
    const Disk disk;
    const RemovedAtTheEnd cleanup(disk.Root());
    const std::filesystem::path destination = disk.AddFolder("Community");
    const std::filesystem::path folder = destination / PathFromUtf8("caf\xC3\xA9-\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E");
    const std::filesystem::path file = destination
        / PathFromUtf8("\xC3\xBC"
                       "ber-\xD0\x96.bgl");
    const std::filesystem::path odd = destination / "bad\xFF\xFE.bgl";
    std::filesystem::create_directories(folder);
    std::ofstream(file) << "hello";
    std::ofstream(odd) << "odd";

    const PosixFilesystemProbe probe;
    std::vector<std::filesystem::path> files = probe.ChildFiles(destination);
    std::ranges::sort(files);
    std::vector<std::filesystem::path> expected{file, odd};
    std::ranges::sort(expected);

    QCOMPARE(probe.ChildDirectories(destination), std::vector<std::filesystem::path>{folder});
    QCOMPARE(files, expected);
    QVERIFY(probe.EntryExistsWithoutFollowingLinks(odd));
}

void PosixFilesystemProbeTest::AFolderThatIsNotThereOrHasNothingInItListsNothing()
{
    const Disk disk;
    const std::filesystem::path empty = disk.AddFolder("empty");

    const PosixFilesystemProbe probe;

    QVERIFY(probe.ChildDirectories(empty).empty());
    QVERIFY(probe.ChildFiles(empty).empty());
    QVERIFY(probe.ChildDirectories(disk.Root() / "never-there").empty());
    QVERIFY(probe.ChildFiles(disk.Root() / "never-there").empty());
}

void PosixFilesystemProbeTest::AnAbsolutePathIsOnAnAvailableVolumeAndARelativeOneIsNot()
{
    const Disk disk;

    const PosixFilesystemProbe probe;

    QVERIFY(probe.VolumeIsAvailable(disk.Root() / "library"));
    QVERIFY(!probe.VolumeIsAvailable("library/fenix"));
    QVERIFY(!probe.VolumeIsAvailable(std::filesystem::path{}));
}

void PosixFilesystemProbeTest::TwoPathsInTheSameFolderShareAVolumeAndAPathNotThereYetFollowsItsParent()
{
    const Disk disk;
    const std::filesystem::path first = disk.AddFolder("library/fenix");
    const std::filesystem::path second = disk.AddFolder("library/pmdg");

    const PosixFilesystemProbe probe;

    QVERIFY(probe.VolumeIdentityOf(first).has_value());
    QCOMPARE(probe.VolumeIdentityOf(first), probe.VolumeIdentityOf(second));
    QCOMPARE(probe.VolumeIdentityOf(disk.Root() / "library" / "never" / "there" / "yet"),
             probe.VolumeIdentityOf(first));
    QVERIFY(ShareAVolume(probe, first, disk.Root() / "library" / "never" / "there" / "yet"));
}

void PosixFilesystemProbeTest::APathOnAnotherFilesystemSharesNoVolumeWithTheDisk()
{
    const Disk disk;
    const std::filesystem::path other = AFolderOnAnotherFilesystem(disk.Root());
    if (other.empty())
    {
        QSKIP("no second filesystem to compare with: the identity of two volumes was not exercised");
    }
    qInfo() << "a second filesystem is mounted at" << AsText(other).toStdString().c_str()
            << ": the identity of two volumes is exercised";
    const RemovedAtTheEnd cleanup(other);

    const PosixFilesystemProbe probe;

    QVERIFY(probe.VolumeIdentityOf(other).has_value());
    QVERIFY(probe.VolumeIdentityOf(other) != probe.VolumeIdentityOf(disk.Root()));
    QVERIFY(!ShareAVolume(probe, other / "landing", disk.Root() / "landing"));
    QVERIFY(ShareAVolume(probe, other / "landing", other / "elsewhere" / "deeper"));
}

void PosixFilesystemProbeTest::ABindMountOfTheSameFilesystemIsAnotherVolume()
{
    const Disk disk;
    const std::filesystem::path original = disk.AddFolder("original");
    const std::filesystem::path mirror = disk.AddFolder("mirror");

    const bool bound = ::mount(original.c_str(), mirror.c_str(), nullptr, MS_BIND, nullptr) == 0;
    qInfo() << (bound ? "this process may mount: a bind mount of the same filesystem must be another volume"
                      : "this process may not mount: two folders of one filesystem must share the volume");
    const PosixFilesystemProbe probe;

    const bool shared = ShareAVolume(probe, original / "landing", mirror / "landing");
    const bool sameDevice = DeviceOf(original) == DeviceOf(mirror);

    if (bound)
    {
        QCOMPARE(::umount2(mirror.c_str(), MNT_DETACH), 0);
    }

    QCOMPARE(shared, !bound);
    QVERIFY(sameDevice);
}

void PosixFilesystemProbeTest::APathThatIsRelativeOrEmptyHasNoVolumeToShare()
{
    const Disk disk;

    const PosixFilesystemProbe probe;

    QVERIFY(!probe.VolumeIdentityOf("library/fenix").has_value());
    QVERIFY(!probe.VolumeIdentityOf(std::filesystem::path{}).has_value());
    QVERIFY(!ShareAVolume(probe, "library/fenix", "library/pmdg"));
    QVERIFY(!ShareAVolume(probe, disk.Root(), "library/pmdg"));
}

void PosixFilesystemProbeTest::AMountThatIsConfiguredButNotMountedMakesTheVolumeUnavailable()
{
    const Disk disk;
    const std::filesystem::path mountInfo = disk.AddFile(
        "mountinfo",
        "28 1 259:2 / / rw - ext4 /dev/a rw\n41 28 8:17 / /run/media/bruno/MOUNTED rw - exfat /dev/b rw\n");
    const std::filesystem::path fstab = disk.AddFile("fstab", "UUID=1 /home/bruno/Jogos ext4 defaults 0 2\n");
    const std::filesystem::path unplugged = disk.Root() / "unplugged";

    const PosixFilesystemProbe probe(mountInfo, fstab);

    QVERIFY(!probe.VolumeIsAvailable("/home/bruno/Jogos/SteamLibrary"));
    QVERIFY(!probe.VolumeIsAvailable("/run/media/bruno/UNPLUGGED/SteamLibrary"));
    QVERIFY(probe.VolumeIsAvailable("/run/media/bruno/MOUNTED/SteamLibrary"));
    QVERIFY(probe.VolumeIsAvailable("/home/bruno/Documents"));
    QVERIFY(probe.VolumeIsAvailable(disk.Root() / "library"));
    QVERIFY(!probe.VolumeIdentityOf("/home/bruno/Jogos/SteamLibrary").has_value());
    QVERIFY(!probe.VolumeIdentityOf("/run/media/bruno/UNPLUGGED/SteamLibrary").has_value());
    QVERIFY(!ShareAVolume(probe, "/home/bruno/Jogos/a", "/home/bruno/Jogos/b"));
    QVERIFY(!probe.VolumeIsAvailable("library/fenix"));
    QVERIFY(PosixFilesystemProbe(unplugged / "no-mountinfo", unplugged / "no-fstab").VolumeIsAvailable("/home/bruno"));
}

void PosixFilesystemProbeTest::EveryMountOfThisMachineIsAnAvailableVolume()
{
    std::ifstream table(kSystemMountInfo, std::ios::binary);
    QVERIFY(table.is_open());
    const std::string text{std::istreambuf_iterator<char>(table), std::istreambuf_iterator<char>()};
    const PosixFilesystemProbe probe;
    std::size_t checked = 0;

    for (const std::filesystem::path& mount : MountedPointsIn(text))
    {
        QVERIFY2(probe.VolumeIsAvailable(mount), mount.c_str());
        QVERIFY2(probe.VolumeIsAvailable(mount / "anything" / "below"), mount.c_str());
        ++checked;
    }

    QVERIFY(checked > 0);
}

void PosixFilesystemProbeTest::AFolderThatIsNotThereIsNotTheSameAsOneThatRefusesTheWrite()
{
    const Disk disk;

    const PosixFilesystemProbe probe;

    QCOMPARE(probe.ProbeWritable(disk.Root() / "never-there"), WriteAccess::TheFolderIsNotThere);
    QCOMPARE(probe.ProbeWritable(disk.AddFile("a-file", "x")), WriteAccess::TheFolderIsNotThere);
}

void PosixFilesystemProbeTest::AFolderThatTakesTheWriteLeavesNoProbeBehind()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("Community");

    const PosixFilesystemProbe probe;

    QCOMPARE(probe.ProbeWritable(folder), WriteAccess::ItAccepts);
    QVERIFY(std::filesystem::is_empty(folder));
}

void PosixFilesystemProbeTest::AFolderTheProcessCannotWriteSaysPermissionIsWhatStoppedIt()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("Community");
    std::filesystem::permissions(folder, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::replace);

    const bool theProcessMayWrite = ::access(folder.c_str(), W_OK) == 0;
    qInfo() << (theProcessMayWrite ? "this process may write to a read-only folder (root): the probe must accept"
                                   : "this process is held back by the folder mode: the probe must say permission");

    const PosixFilesystemProbe probe;
    const WriteAccess expected = theProcessMayWrite ? WriteAccess::ItAccepts : WriteAccess::PermissionIsDenied;

    QCOMPARE(probe.ProbeWritable(folder), expected);

    std::filesystem::permissions(folder, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
}

void PosixFilesystemProbeTest::AProbeNameTakenByAFolderIsARefusalForAnotherReason()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("Community");
    static_cast<void>(disk.AddFolder("Community/.fsorg-write-probe"));

    const PosixFilesystemProbe probe;

    QCOMPARE(probe.ProbeWritable(folder), WriteAccess::ItRefusedForAnotherReason);
}

void PosixFilesystemProbeTest::NothingHoldsAFileOpenBecauseUnlinkingAnOpenFileWorksOnLinux()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("fenix");
    const std::filesystem::path file = disk.AddFile("fenix/aircraft.cfg", "x");
    std::ifstream holding(file, std::ios::binary);
    QVERIFY(holding.is_open());

    const PosixFilesystemProbe probe;

    QVERIFY(!probe.SomethingIsHoldingItOpen(folder));
    QVERIFY(!probe.SomethingIsHoldingItOpen(file));
}

void PosixFilesystemProbeTest::FreeSpaceIsOnlyAnswerableForAFolderThatAlreadyExists()
{
    const Disk disk;

    const PosixFilesystemProbe probe;

    const std::optional<std::uintmax_t> room = probe.FreeSpaceOn(disk.Root());
    QVERIFY(room.has_value());
    QVERIFY(*room > 0);
    QVERIFY(!probe.FreeSpaceOn(disk.Root() / "never-there").has_value());
}

void PosixFilesystemProbeTest::TheTrashReportsNoQuotaAndNoPathCeiling()
{
    const Disk disk;

    const PosixFilesystemProbe probe;

    const std::optional<RecycleBinRoom> room = probe.RecycleBinOn(disk.Root());

    QVERIFY(room.has_value());
    QVERIFY(!room->quota.has_value());
    QVERIFY(room->itRecycles);
    QVERIFY(!probe.RecycleBinPathCeiling().has_value());
}

void PosixFilesystemProbeTest::ContentsAndFirstBytesComeBackByteForByte()
{
    const Disk disk;
    const std::string bytes = std::string("MZ\0\xFF\r\nrest of the file", 22);
    const std::filesystem::path file = disk.AddFile("fenix/layout.json", bytes);

    const PosixFilesystemProbe probe;

    QCOMPARE(probe.ContentsOf(file), std::optional(bytes));
    QCOMPARE(probe.FirstBytesOf(file, 4), std::optional(bytes.substr(0, 4)));
    QCOMPARE(probe.FirstBytesOf(file, 1000), std::optional(bytes));
    QVERIFY(!probe.ContentsOf(disk.Root() / "never-there").has_value());
    QVERIFY(!probe.FirstBytesOf(disk.Root() / "never-there", 4).has_value());
}

void PosixFilesystemProbeTest::AFolderIsNotReadAsAFile()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("fenix");

    const PosixFilesystemProbe probe;

    QVERIFY(!probe.ContentsOf(folder).has_value());
    QVERIFY(!probe.FirstBytesOf(folder, 4).has_value());
    QVERIFY(!probe.HashOf(folder).has_value());
}

void PosixFilesystemProbeTest::TheHashIsTheSha256OfTheContents()
{
    const Disk disk;
    const std::filesystem::path file = disk.AddFile("abc.txt", "abc");
    const std::filesystem::path empty = disk.AddFile("empty.txt", "");

    const PosixFilesystemProbe probe;

    QCOMPARE(probe.HashOf(file),
             std::optional<std::string>("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    QCOMPARE(probe.HashOf(empty),
             std::optional<std::string>("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

void PosixFilesystemProbeTest::ManyHashesComeBackEachInItsPlaceAndAMissingFileAnswersNothing()
{
    const Disk disk;
    std::vector<std::filesystem::path> below;
    for (int number = 0; number < 40; ++number)
    {
        const std::string name = "files/" + std::to_string(number) + ".bin";
        static_cast<void>(disk.AddFile(name, "content " + std::to_string(number)));
        below.emplace_back(name);
    }

    below.insert(below.begin() + 7, "files/missing.bin");

    const PosixFilesystemProbe probe;
    const std::vector<std::optional<std::string>> hashes = probe.HashesOf(disk.Root(), below);

    QCOMPARE(hashes.size(), below.size());

    for (std::size_t place = 0; place < below.size(); ++place)
    {
        QCOMPARE(hashes[place], probe.HashOf(disk.Root() / below[place]));
    }

    QVERIFY(!hashes[7].has_value());
    QVERIFY(hashes[6].has_value());
    QVERIFY(hashes[6] != hashes[8]);
    QVERIFY(probe.HashesOf(disk.Root(), {}).empty());
}

void PosixFilesystemProbeTest::TheFingerprintNamesEachFileRelativeToTheRootWhetherOrNotTheRootEndsInASeparator()
{
    const Disk disk;
    static_cast<void>(disk.AddFile("fenix/aircraft.cfg", "12345"));
    static_cast<void>(disk.AddFile("fenix/SimObjects/deep/model.mdl", "1234567"));
    static_cast<void>(disk.AddFolder("fenix/empty"));

    const PosixFilesystemProbe probe;
    const std::filesystem::path root = disk.Root() / "fenix";

    for (const std::filesystem::path& spelled : {root, root / ""})
    {
        const std::optional<TreeFingerprint> walked = probe.FingerprintTree(spelled);
        QVERIFY(walked.has_value());

        std::vector<std::pair<std::filesystem::path, std::uintmax_t>> seen;
        for (const FileFingerprint& file : walked->files)
        {
            seen.emplace_back(file.relativePath, file.size);
        }

        std::ranges::sort(seen);

        const std::vector<std::pair<std::filesystem::path, std::uintmax_t>> expected{{"SimObjects/deep/model.mdl", 7},
                                                                                     {"aircraft.cfg", 5}};

        QCOMPARE(seen, expected);
    }
}

void PosixFilesystemProbeTest::TheFingerprintCarriesTheWriteTimeOfEachFileAsTheFileSystemHoldsIt()
{
    const Disk disk;
    const std::filesystem::path file = disk.AddFile("fenix/aircraft.cfg", "12345");
    const std::filesystem::file_time_type moved =
        std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * 400);
    std::filesystem::last_write_time(file, moved);

    const PosixFilesystemProbe probe;
    const std::optional<TreeFingerprint> walked = probe.FingerprintTree(disk.Root() / "fenix");

    QVERIFY(walked.has_value());
    QCOMPARE(walked->files.size(), std::size_t{1});
    QCOMPARE(walked->files.front().lastWriteTime, SystemTimeOf(std::filesystem::last_write_time(file)));
    QVERIFY(walked->files.front().lastWriteTime < std::chrono::system_clock::now() - std::chrono::hours(24 * 399));
}

void PosixFilesystemProbeTest::TheLongestEntryOfAFingerprintIsTheLongestPathUnderTheRoot()
{
    const Disk disk;
    const std::filesystem::path root = disk.AddFolder("fenix");
    static_cast<void>(disk.AddFile("fenix/a.cfg", "x"));
    const std::filesystem::path deepest = disk.AddFile("fenix/nested/deeper/the-longest-name.mdl", "x");

    const PosixFilesystemProbe probe;
    const std::optional<TreeFingerprint> walked = probe.FingerprintTree(root);

    QVERIFY(walked.has_value());
    QCOMPARE(walked->longestEntry, deepest.native().size());

    const Disk bare;
    const std::optional<TreeFingerprint> nothing = probe.FingerprintTree(bare.Root());
    QVERIFY(nothing.has_value());
    QCOMPARE(nothing->longestEntry, bare.Root().native().size());
}

void PosixFilesystemProbeTest::TheFingerprintOfARootThatIsNotThereAnswersNothing()
{
    const Disk disk;

    const PosixFilesystemProbe probe;

    QVERIFY(!probe.FingerprintTree(disk.Root() / "never-there").has_value());
}

void PosixFilesystemProbeTest::AFolderReportsWhenItWasLastWrittenTo()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("fenix");
    const std::filesystem::file_time_type moved =
        std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * 30);
    std::filesystem::last_write_time(folder, moved);

    struct stat found
    {
    };

    QCOMPARE(::stat(folder.c_str(), &found), 0);

    const PosixFilesystemProbe probe;
    const std::optional<std::chrono::system_clock::time_point> written = probe.LastWriteTime(folder);

    QVERIFY(written.has_value());
    QCOMPARE(std::chrono::duration_cast<std::chrono::seconds>(written->time_since_epoch()).count(),
             static_cast<std::int64_t>(found.st_mtim.tv_sec));
    QVERIFY(!probe.LastWriteTime(disk.Root() / "never-there").has_value());
}

QTEST_APPLESS_MAIN(PosixFilesystemProbeTest)

#include "tst_posix_filesystem_probe.moc"
