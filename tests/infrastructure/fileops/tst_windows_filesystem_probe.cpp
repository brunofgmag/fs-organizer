#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <windows.h>

#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/StdFilesystemProbe.h"

class WindowsFilesystemProbeTest : public QObject
{
    Q_OBJECT

private slots:
    static void ADanglingJunctionIsStillAnEntryThatOccupiesItsPath();
    static void ChildDirectoriesReportsDanglingJunctionsToo();
    static void AnUnmountedDriveLetterIsNotAnAvailableVolume();
    static void AJunctionIsAReparsePointAndARealFolderIsNot();
    static void FreeSpaceIsOnlyAnswerableForAFolderThatAlreadyExists();
    static void AFolderReportsWhenItWasLastWrittenTo();
    static void TheStandardLibraryDoubleAnswersAJunctionTheSameWayThisProbeDoes();
};

namespace
{
    struct Disk
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return {directory.path().toStdString()};
        }

        [[nodiscard]] std::filesystem::path AddFolder(const std::string& relativePath) const
        {
            const std::filesystem::path folder = Root() / relativePath;
            std::filesystem::create_directories(folder);
            return folder;
        }

        [[nodiscard]] std::filesystem::path AddDanglingJunction(const std::string& relativePath) const
        {
            const std::filesystem::path target = AddFolder("doomed-target");
            const std::filesystem::path linkPath = Root() / relativePath;
            std::filesystem::create_directories(linkPath.parent_path());

            WindowsLinkService linkService;
            [&]
            {
                QCOMPARE(linkService.CreateLink(linkPath, target, LinkType::Junction), LinkFailure::None);
            }();
            std::filesystem::remove_all(target);

            return linkPath;
        }
    };
}

void WindowsFilesystemProbeTest::ADanglingJunctionIsStillAnEntryThatOccupiesItsPath()
{
    const Disk disk;
    const std::filesystem::path linkPath = disk.AddDanglingJunction("Community/ag-airport-bgqq");

    const WindowsFilesystemProbe filesystemProbe;

    QVERIFY(!std::filesystem::exists(linkPath));
    QVERIFY(filesystemProbe.EntryExistsWithoutFollowingLinks(linkPath));
    QVERIFY(!filesystemProbe.TargetDirectoryExists(linkPath));
}

void WindowsFilesystemProbeTest::ChildDirectoriesReportsDanglingJunctionsToo()
{
    const Disk disk;
    const std::filesystem::path destination = disk.AddFolder("Community");
    const std::filesystem::path physical = disk.AddFolder("Community/asfs");
    const std::filesystem::path dangling = disk.AddDanglingJunction("Community/ag-airport-bgqq");

    const WindowsFilesystemProbe filesystemProbe;
    const std::vector<std::filesystem::path> children = filesystemProbe.ChildDirectories(destination);

    QCOMPARE(children.size(), std::size_t{2});
    QVERIFY(std::ranges::find(children, physical) != children.end());
    QVERIFY(std::ranges::find(children, dangling) != children.end());
}

void WindowsFilesystemProbeTest::AnUnmountedDriveLetterIsNotAnAvailableVolume()
{
    const DWORD mounted = GetLogicalDrives();
    char absentLetter = 0;
    for (char letter = 'Z'; letter >= 'D'; --letter)
    {
        if ((mounted & (1u << (letter - 'A'))) == 0)
        {
            absentLetter = letter;
            break;
        }
    }
    QVERIFY2(absentLetter != 0, "every drive letter from D to Z is mounted on this machine");

    const Disk disk;
    const WindowsFilesystemProbe filesystemProbe;

    const auto absent = std::filesystem::path(std::string(1, absentLetter) + ":/Portable Library/orbx-airport");

    QVERIFY(!filesystemProbe.VolumeIsAvailable(absent));
    QVERIFY(filesystemProbe.VolumeIsAvailable(disk.Root()));
}

void WindowsFilesystemProbeTest::AJunctionIsAReparsePointAndARealFolderIsNot()
{
    const Disk disk;
    const std::filesystem::path physical = disk.AddFolder("Community/asfs");
    const std::filesystem::path dangling = disk.AddDanglingJunction("Community/ag-airport-bgqq");

    const WindowsFilesystemProbe filesystemProbe;

    QVERIFY(filesystemProbe.IsReparsePoint(dangling));
    QVERIFY(!filesystemProbe.IsReparsePoint(physical));
    QVERIFY(!filesystemProbe.IsReparsePoint(disk.Root() / "Community/never-created"));
}

void WindowsFilesystemProbeTest::FreeSpaceIsOnlyAnswerableForAFolderThatAlreadyExists()
{
    const Disk disk;
    const std::filesystem::path category = disk.AddFolder("Utils");

    const WindowsFilesystemProbe filesystemProbe;

    QVERIFY(filesystemProbe.FreeSpaceOn(category).has_value());
    QVERIFY(!filesystemProbe.FreeSpaceOn(category / "flybywire-externaltools-simbridge").has_value());
}

void WindowsFilesystemProbeTest::AFolderReportsWhenItWasLastWrittenTo()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("Utils/simbridge");

    const WindowsFilesystemProbe filesystemProbe;
    const std::optional<std::chrono::system_clock::time_point> written = filesystemProbe.LastWriteTime(folder);

    QVERIFY(written.has_value());
    QVERIFY(std::chrono::abs(std::chrono::system_clock::now() - *written) < std::chrono::minutes{5});
    QVERIFY(!filesystemProbe.LastWriteTime(folder / "never-existed").has_value());
}

void WindowsFilesystemProbeTest::TheStandardLibraryDoubleAnswersAJunctionTheSameWayThisProbeDoes()
{
    const Disk disk;
    const std::filesystem::path destination = disk.AddFolder("Community");
    const std::filesystem::path physical = disk.AddFolder("Community/asfs");
    const std::filesystem::path live = disk.AddFolder("live-target");
    const std::filesystem::path liveLink = destination / "ag-airport-live";

    WindowsLinkService linkService;
    QCOMPARE(linkService.CreateLink(liveLink, live, LinkType::Junction), LinkFailure::None);

    const std::filesystem::path dangling = disk.AddDanglingJunction("Community/ag-airport-bgqq");

    const WindowsFilesystemProbe production;
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

QTEST_APPLESS_MAIN(WindowsFilesystemProbeTest)

#include "tst_windows_filesystem_probe.moc"
