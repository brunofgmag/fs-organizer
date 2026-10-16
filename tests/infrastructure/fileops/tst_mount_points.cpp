#include <QtTest/QtTest>

#include <filesystem>
#include <string>
#include <vector>

#include "infrastructure/fileops/MountPoints.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class MountPointsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheMountedPointsAreTheFifthFieldOfEachLineWithItsEscapesUndone();
        static void TheConfiguredPointsSkipCommentsSwapAndWhatIsNotAbsolute();
        static void APathUnderAConfiguredMountThatIsNotMountedIsMissing();
        static void APathUnderAConfiguredMountThatIsMountedIsPresent();
        static void AMountDeeperThanTheConfiguredOneKeepsThePathPresent();
        static void AConfiguredMountWithASpaceInItsNameIsFoundWhateverTheEscape();
        static void ADiskOfTheDesktopThatIsNotMountedIsMissing();
        static void ADiskOfTheDesktopThatIsMountedIsPresent();
        static void AnOldStyleMountDirectlyUnderMediaKeepsItsSubfoldersPresent();
        static void AFolderThatNothingConfiguresIsPresentWhereverItIs();
        static void ThePathIsJudgedByItsNormalFormAndNotByHowItIsTyped();
        static void NothingKnownMeansNothingIsMissing();
    };
}

namespace
{
    const std::string kMountInfo =
        "22 28 0:21 / /sys rw,nosuid,nodev,noexec,relatime shared:7 - sysfs sysfs rw\n"
        "28 1 259:2 / / rw,relatime shared:1 - ext4 /dev/nvme0n1p2 rw\n"
        "41 28 259:3 / /home/bruno/My\\040Games rw,relatime shared:30 - ext4 /dev/nvme1n1p1 rw\n"
        "\n";

    const std::string kFstab = "# <file system> <mount point> <type> <options> <dump> <pass>\n"
                               "UUID=1111 / ext4 defaults 0 1\n"
                               "UUID=2222 /home/bruno/Jogos ext4 defaults 0 2\n"
                               "UUID=3333 none swap defaults 0 0\n"
                               "UUID=4444 swap swap defaults 0 0\n"
                               "UUID=5555 /mnt/data\\040disk ext4 noauto 0 0\n"
                               "   # indented comment /mnt/never ext4 defaults 0 0\n"
                               "tmpfs relative tmpfs defaults 0 0\n";

    using Paths = std::vector<std::filesystem::path>;
}

void MountPointsTest::TheMountedPointsAreTheFifthFieldOfEachLineWithItsEscapesUndone()
{
    QCOMPARE(MountedPointsIn(kMountInfo), (Paths{"/sys", "/", "/home/bruno/My Games"}));
    QCOMPARE(MountedPointsIn(""), Paths{});
    QCOMPARE(MountedPointsIn("garbage\n1 2 3\n"), Paths{});
}

void MountPointsTest::TheConfiguredPointsSkipCommentsSwapAndWhatIsNotAbsolute()
{
    QCOMPARE(ConfiguredPointsIn(kFstab), (Paths{"/", "/home/bruno/Jogos", "/mnt/data disk"}));
    QCOMPARE(ConfiguredPointsIn(""), Paths{});
}

void MountPointsTest::APathUnderAConfiguredMountThatIsNotMountedIsMissing()
{
    const Paths mounted = MountedPointsIn(kMountInfo);
    const Paths configured = ConfiguredPointsIn(kFstab);

    QVERIFY(TheMountIsMissing("/home/bruno/Jogos", mounted, configured));
    QVERIFY(TheMountIsMissing("/home/bruno/Jogos/SteamLibrary/steamapps", mounted, configured));
    QVERIFY(TheMountIsMissing("/mnt/data disk/Addons", mounted, configured));
}

void MountPointsTest::APathUnderAConfiguredMountThatIsMountedIsPresent()
{
    const Paths mounted = MountedPointsIn("28 1 259:2 / / rw - ext4 /dev/a rw\n"
                                          "41 28 259:3 / /home/bruno/Jogos rw - ext4 /dev/b rw\n");
    const Paths configured = ConfiguredPointsIn(kFstab);

    QVERIFY(!TheMountIsMissing("/home/bruno/Jogos", mounted, configured));
    QVERIFY(!TheMountIsMissing("/home/bruno/Jogos/SteamLibrary", mounted, configured));
    QVERIFY(!TheMountIsMissing("/home/bruno/Documents", mounted, configured));
    QVERIFY(!TheMountIsMissing("/home/bruno/JogosEmOutroLugar/x", mounted, configured));
}

void MountPointsTest::AMountDeeperThanTheConfiguredOneKeepsThePathPresent()
{
    const Paths mounted = MountedPointsIn("28 1 259:2 / / rw - ext4 /dev/a rw\n"
                                          "41 28 259:3 / /home/bruno/Jogos/SteamLibrary rw - ext4 /dev/b rw\n");

    QVERIFY(!TheMountIsMissing("/home/bruno/Jogos/SteamLibrary/steamapps", mounted, ConfiguredPointsIn(kFstab)));
    QVERIFY(TheMountIsMissing("/home/bruno/Jogos/Other", mounted, ConfiguredPointsIn(kFstab)));
}

void MountPointsTest::AConfiguredMountWithASpaceInItsNameIsFoundWhateverTheEscape()
{
    const Paths mounted = MountedPointsIn("28 1 259:2 / / rw - ext4 /dev/a rw\n"
                                          "41 28 259:3 / /mnt/data\\040disk rw - ext4 /dev/b rw\n");

    QVERIFY(!TheMountIsMissing("/mnt/data disk/Addons", mounted, ConfiguredPointsIn(kFstab)));
    QVERIFY(TheMountIsMissing("/mnt/data disk/Addons", MountedPointsIn("28 1 259:2 / / rw - ext4 /dev/a rw\n"),
                              ConfiguredPointsIn(kFstab)));
}

void MountPointsTest::ADiskOfTheDesktopThatIsNotMountedIsMissing()
{
    const Paths mounted = MountedPointsIn("28 1 259:2 / / rw - ext4 /dev/a rw\n");

    QVERIFY(TheMountIsMissing("/run/media/bruno/GAMES", mounted, {}));
    QVERIFY(TheMountIsMissing("/run/media/bruno/GAMES/SteamLibrary", mounted, {}));
    QVERIFY(TheMountIsMissing("/media/bruno/GAMES/SteamLibrary", mounted, {}));
    QVERIFY(!TheMountIsMissing("/run/media/bruno", mounted, {}));
    QVERIFY(!TheMountIsMissing("/run/media", mounted, {}));
    QVERIFY(!TheMountIsMissing("/run/media/bruno/", mounted, {}));
}

void MountPointsTest::ADiskOfTheDesktopThatIsMountedIsPresent()
{
    const Paths mounted = MountedPointsIn("28 1 259:2 / / rw - ext4 /dev/a rw\n"
                                          "41 28 8:17 / /run/media/bruno/GAMES rw - exfat /dev/sdb1 rw\n");

    QVERIFY(!TheMountIsMissing("/run/media/bruno/GAMES", mounted, {}));
    QVERIFY(!TheMountIsMissing("/run/media/bruno/GAMES/SteamLibrary", mounted, {}));
    QVERIFY(TheMountIsMissing("/run/media/bruno/OTHER/SteamLibrary", mounted, {}));
}

void MountPointsTest::AnOldStyleMountDirectlyUnderMediaKeepsItsSubfoldersPresent()
{
    const Paths mounted = MountedPointsIn("28 1 259:2 / / rw - ext4 /dev/a rw\n"
                                          "41 28 8:17 / /media/usbdisk rw - vfat /dev/sdb1 rw\n");

    QVERIFY(!TheMountIsMissing("/media/usbdisk/Addons", mounted, {}));
    QVERIFY(!TheMountIsMissing("/media/usbdisk/Addons/deeper", mounted, {}));
}

void MountPointsTest::AFolderThatNothingConfiguresIsPresentWhereverItIs()
{
    const Paths mounted = MountedPointsIn("28 1 259:2 / / rw - ext4 /dev/a rw\n");

    QVERIFY(!TheMountIsMissing("/mnt/plain/folder", mounted, ConfiguredPointsIn(kFstab)));
    QVERIFY(!TheMountIsMissing("/home/bruno/Community", mounted, ConfiguredPointsIn(kFstab)));
    QVERIFY(!TheMountIsMissing("/", mounted, ConfiguredPointsIn(kFstab)));
}

void MountPointsTest::ThePathIsJudgedByItsNormalFormAndNotByHowItIsTyped()
{
    const Paths mounted = MountedPointsIn("28 1 259:2 / / rw - ext4 /dev/a rw\n");
    const Paths configured = ConfiguredPointsIn(kFstab);

    QVERIFY(TheMountIsMissing("/home/bruno/./Jogos//SteamLibrary/", mounted, configured));
    QVERIFY(TheMountIsMissing("/home/bruno/Documents/../Jogos/SteamLibrary", mounted, configured));
    QVERIFY(!TheMountIsMissing("/home/bruno/Jogos/../Documents", mounted, configured));
}

void MountPointsTest::NothingKnownMeansNothingIsMissing()
{
    QVERIFY(!TheMountIsMissing("/home/bruno/Jogos", {}, ConfiguredPointsIn(kFstab)));
    QVERIFY(!TheMountIsMissing("/run/media/bruno/GAMES", {}, {}));
}

QTEST_APPLESS_MAIN(MountPointsTest)

#include "tst_mount_points.moc"
