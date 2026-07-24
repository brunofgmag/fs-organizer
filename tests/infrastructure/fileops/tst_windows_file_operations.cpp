#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <windows.h>

#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "tests/support/PathPrinting.h"

class WindowsFileOperationsTest : public QObject
{
    Q_OBJECT

private slots:
    static void ADanglingJunctionIsStillAnEntryThatOccupiesItsPath();
    static void ChildDirectoriesReportsDanglingJunctionsToo();
    static void AnUnmountedDriveLetterIsNotAnAvailableVolume();
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
            [&] { QVERIFY(linkService.CreateLink(linkPath, target, LinkType::Junction)); }();
            std::filesystem::remove_all(target);

            return linkPath;
        }
    };
}

void WindowsFileOperationsTest::ADanglingJunctionIsStillAnEntryThatOccupiesItsPath()
{
    const Disk disk;
    const std::filesystem::path linkPath = disk.AddDanglingJunction("Community/ag-airport-bgqq");

    const WindowsFileOperations fileOperations;

    QVERIFY(!std::filesystem::exists(linkPath));
    QVERIFY(fileOperations.EntryExistsWithoutFollowingLinks(linkPath));
    QVERIFY(!fileOperations.TargetDirectoryExists(linkPath));
}

void WindowsFileOperationsTest::ChildDirectoriesReportsDanglingJunctionsToo()
{
    const Disk disk;
    const std::filesystem::path destination = disk.AddFolder("Community");
    const std::filesystem::path physical = disk.AddFolder("Community/asfs");
    const std::filesystem::path dangling = disk.AddDanglingJunction("Community/ag-airport-bgqq");

    const WindowsFileOperations fileOperations;
    const std::vector<std::filesystem::path> children = fileOperations.ChildDirectories(destination);

    QCOMPARE(children.size(), std::size_t{2});
    QVERIFY(std::ranges::find(children, physical) != children.end());
    QVERIFY(std::ranges::find(children, dangling) != children.end());
}

void WindowsFileOperationsTest::AnUnmountedDriveLetterIsNotAnAvailableVolume()
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
    const WindowsFileOperations fileOperations;

    const auto absent =
        std::filesystem::path(std::string(1, absentLetter) + ":/Portable Library/orbx-airport");

    QVERIFY(!fileOperations.VolumeIsAvailable(absent));
    QVERIFY(fileOperations.VolumeIsAvailable(disk.Root()));
}

QTEST_APPLESS_MAIN(WindowsFileOperationsTest)

#include "tst_windows_file_operations.moc"
