#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <windows.h>

#include <fstream>

#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class WindowsFileOperationsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void RemovingATreeAtAJunctionNeverReachesTheTarget();
        static void RemovingATreeTakesTheWholeSubtree();
        static void CopyingATreeReproducesEveryFileAndItsSize();
        static void CopyingASourceThatCannotBeWalkedFailsInsteadOfLandingNothing();
        static void CancellingTheProgressCallbackStopsTheCopy();
        static void MovingAcrossVolumesIsRefusedInsteadOfCopied();
        static void MovingIntoAFolderThatDoesNotExistYetOpensTheWayThere();
    };
}

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

        [[nodiscard]] std::filesystem::path AddFile(const std::string& relativePath, const std::string& content) const
        {
            const std::filesystem::path file = Root() / relativePath;
            std::filesystem::create_directories(file.parent_path());
            std::ofstream(file, std::ios::binary) << content;
            return file;
        }
    };

    [[nodiscard]] char SomeOtherFixedVolume(const std::filesystem::path& avoided)
    {
        const std::string avoidedRoot = avoided.root_name().string();
        const DWORD mounted = GetLogicalDrives();

        for (char letter = 'A'; letter <= 'Z'; ++letter)
        {
            if ((mounted & (1u << (letter - 'A'))) == 0)
            {
                continue;
            }

            const std::string root = std::string(1, letter) + ":\\";
            if (root.substr(0, 2) == avoidedRoot || GetDriveTypeA(root.c_str()) != DRIVE_FIXED)
            {
                continue;
            }

            return letter;
        }

        return 0;
    }
}

void WindowsFileOperationsTest::RemovingATreeAtAJunctionNeverReachesTheTarget()
{
    const Disk disk;
    const std::filesystem::path target = disk.AddFolder("Library/aerosoft-crj");
    const std::filesystem::path manifest = disk.AddFile("Library/aerosoft-crj/manifest.json", "{}");
    const std::filesystem::path deep = disk.AddFile("Library/aerosoft-crj/SimObjects/model.gltf", "vertices");

    const std::filesystem::path linkPath = disk.Root() / "Community/aerosoft-crj";
    std::filesystem::create_directories(linkPath.parent_path());

    WindowsLinkService linkService;
    QCOMPARE(linkService.CreateLink(linkPath, target, LinkType::Junction), LinkFailure::None);

    WindowsFileOperations files;
    QVERIFY(files.RemoveTree(linkPath));

    QVERIFY(!std::filesystem::exists(linkPath));
    QVERIFY(std::filesystem::exists(target));
    QVERIFY(std::filesystem::exists(manifest));
    QVERIFY(std::filesystem::exists(deep));
}

void WindowsFileOperationsTest::RemovingATreeTakesTheWholeSubtree()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("Community/asfs");
    static_cast<void>(disk.AddFile("Community/asfs/manifest.json", "{}"));
    static_cast<void>(disk.AddFile("Community/asfs/scenery/objects.bgl", "bytes"));

    WindowsFileOperations files;
    QVERIFY(files.RemoveTree(folder));

    QVERIFY(!std::filesystem::exists(folder));
    QVERIFY(std::filesystem::exists(disk.Root() / "Community"));
}

void WindowsFileOperationsTest::CopyingATreeReproducesEveryFileAndItsSize()
{
    const Disk disk;
    const std::filesystem::path source = disk.AddFolder("Community/asfs");
    static_cast<void>(disk.AddFile("Community/asfs/manifest.json", "{}"));
    static_cast<void>(disk.AddFile("Community/asfs/scenery/objects.bgl", "bytes-and-bytes"));

    const std::filesystem::path landing = disk.Root() / "Library/asfs.fsorg-partial";

    WindowsFileOperations files;
    QCOMPARE(files.CopyTree(source, landing, {}), CopyOutcome::Completed);

    QVERIFY(std::filesystem::exists(landing / "manifest.json"));
    QVERIFY(std::filesystem::exists(landing / "scenery/objects.bgl"));
    QCOMPARE(std::filesystem::file_size(landing / "scenery/objects.bgl"), std::uintmax_t{15});
    QVERIFY(std::filesystem::exists(source / "manifest.json"));
}

void WindowsFileOperationsTest::CopyingASourceThatCannotBeWalkedFailsInsteadOfLandingNothing()
{
    const Disk disk;
    const std::filesystem::path neverCreated = disk.Root() / "Community/asfs";
    const std::filesystem::path landing = disk.Root() / "Library/asfs.fsorg-partial";

    WindowsFileOperations files;
    QCOMPARE(files.CopyTree(neverCreated, landing, {}), CopyOutcome::Failed);
    QVERIFY(!std::filesystem::exists(landing));
}

void WindowsFileOperationsTest::CancellingTheProgressCallbackStopsTheCopy()
{
    const Disk disk;
    const std::filesystem::path source = disk.AddFolder("Community/asfs");
    static_cast<void>(disk.AddFile("Community/asfs/a.bin", std::string(4096, 'a')));
    static_cast<void>(disk.AddFile("Community/asfs/b.bin", std::string(4096, 'b')));

    const std::filesystem::path landing = disk.Root() / "Library/asfs.fsorg-partial";

    WindowsFileOperations files;
    const CopyOutcome outcome = files.CopyTree(source, landing,
                                               [](const CopyProgress&)
                                               {
                                                   return false;
                                               });

    QCOMPARE(outcome, CopyOutcome::Cancelled);
    QVERIFY(std::filesystem::exists(source / "a.bin"));
    QVERIFY(std::filesystem::exists(source / "b.bin"));
}

void WindowsFileOperationsTest::MovingAcrossVolumesIsRefusedInsteadOfCopied()
{
    const Disk disk;
    const std::filesystem::path source = disk.AddFolder("Community/asfs");
    static_cast<void>(disk.AddFile("Community/asfs/manifest.json", "{}"));

    const char other = SomeOtherFixedVolume(disk.Root());
    if (other == 0)
    {
        QSKIP("this machine has a single fixed volume, so the refusal cannot be observed");
    }

    const auto landing = std::filesystem::path(std::string(1, other) + ":/fsorg-cross-volume-move-probe");

    WindowsFileOperations files;
    QVERIFY(!files.Move(source, landing));

    QVERIFY(std::filesystem::exists(source / "manifest.json"));
    QVERIFY(!std::filesystem::exists(landing));
}

void WindowsFileOperationsTest::MovingIntoAFolderThatDoesNotExistYetOpensTheWayThere()
{
    const Disk disk;
    const std::filesystem::path source = disk.AddFolder("Library/Utils/tfdi-md11");
    static_cast<void>(disk.AddFile("Library/Utils/tfdi-md11/manifest.json", "{}"));

    const std::filesystem::path landing = disk.Root() / "Library" / "_fsorganizer-quarantine" / "tfdi-md11";
    QVERIFY(!std::filesystem::exists(landing.parent_path()));

    WindowsFileOperations files;
    QVERIFY(files.Move(source, landing));

    QVERIFY(std::filesystem::exists(landing / "manifest.json"));
    QVERIFY(!std::filesystem::exists(source));
}

QTEST_APPLESS_MAIN(WindowsFileOperationsTest)

#include "tst_windows_file_operations.moc"
