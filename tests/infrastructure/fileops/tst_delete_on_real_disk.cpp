#include <QtTest/QtTest>

#include <fstream>
#include <string>
#include <system_error>

#include "domain/model/RecycleLimits.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "tests/support/DeepPaths.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/TempFiles.h"

namespace
{
    class DeleteOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AShallowFolderReachesTheRecycleBinAndCanBeFoundThere();
        static void AFolderPastTheCeilingIsRefusedInsteadOfDestroyedInSilence();
        static void ThePermanentRouteStillReachesPastTheCeiling();
        static void TheLongestEntryIsMeasuredOverTheWholeTree();
        static void TheRecycleBinOfAVolumeThatIsNotThereIsNotInvented();
    };

    std::filesystem::path RecycleBinOf(const std::filesystem::path& path)
    {
        return path.root_path() / "$Recycle.Bin";
    }

    std::size_t ItemsInTheRecycleBin(const std::filesystem::path& volume)
    {
        std::error_code error;
        std::size_t items = 0;

        for (const std::filesystem::directory_entry& owner : std::filesystem::directory_iterator(
                 RecycleBinOf(volume), std::filesystem::directory_options::skip_permission_denied, error))
        {
            std::error_code inside;
            for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(
                     owner.path(), std::filesystem::directory_options::skip_permission_denied, inside))
            {
                if (entry.path().filename().wstring().starts_with(L"$R"))
                {
                    ++items;
                }
            }
        }

        return items;
    }

    std::filesystem::path AnAddonFolderIn(const std::filesystem::path& root, const std::string& name)
    {
        const std::filesystem::path folder = root / name;
        std::filesystem::create_directories(folder);

        std::ofstream(folder / "manifest.json") << R"({"title":"probe"})";

        return folder;
    }
}

void DeleteOnRealDiskTest::AShallowFolderReachesTheRecycleBinAndCanBeFoundThere()
{
    const TempFiles files;
    const std::filesystem::path addon = AnAddonFolderIn(files.Root(), "aerosoft-crj");

    const std::size_t before = ItemsInTheRecycleBin(files.Root());

    WindowsFileOperations operations;
    QVERIFY(operations.Recycle(addon));

    QVERIFY(!std::filesystem::exists(addon));
    QCOMPARE(ItemsInTheRecycleBin(files.Root()), before + 1);
}

void DeleteOnRealDiskTest::AFolderPastTheCeilingIsRefusedInsteadOfDestroyedInSilence()
{
    const TempFiles files;
    const std::filesystem::path addon = FolderPastTheCeiling(files.Root(), "hype-atr");
    WriteFilePastTheCeiling(addon / "manifest.json", R"({"title":"probe"})");

    QVERIFY(addon.wstring().size() > kTheRecycleBinStopsAt);

    const std::size_t before = ItemsInTheRecycleBin(files.Root());

    WindowsFileOperations operations;
    QVERIFY(!operations.Recycle(addon));

    QVERIFY(ExistsPastTheCeiling(addon));
    QCOMPARE(ItemsInTheRecycleBin(files.Root()), before);

    RemovePastTheCeiling(files.Root() / std::string(60, 'x'));
}

void DeleteOnRealDiskTest::ThePermanentRouteStillReachesPastTheCeiling()
{
    const TempFiles files;
    const std::filesystem::path addon = FolderPastTheCeiling(files.Root(), "hype-atr");
    WriteFilePastTheCeiling(addon / "manifest.json", R"({"title":"probe"})");

    WindowsFileOperations operations;
    QVERIFY(operations.RemoveTree(addon));

    QVERIFY(!ExistsPastTheCeiling(addon));

    RemovePastTheCeiling(files.Root() / std::string(60, 'x'));
}

void DeleteOnRealDiskTest::TheLongestEntryIsMeasuredOverTheWholeTree()
{
    const TempFiles files;
    const std::filesystem::path addon = AnAddonFolderIn(files.Root(), "aerosoft-crj");
    const std::filesystem::path buried = addon / "SimObjects" / "Airplanes" / "CRJ700" / "model";
    std::filesystem::create_directories(buried);
    std::ofstream(buried / "aircraft.cfg") << "probe";

    const WindowsFilesystemProbe probe;
    const std::optional<std::size_t> longest = probe.LongestEntryUnder(addon);

    QVERIFY(longest.has_value());
    QCOMPARE(*longest, (buried / "aircraft.cfg").wstring().size());
}

void DeleteOnRealDiskTest::TheRecycleBinOfAVolumeThatIsNotThereIsNotInvented()
{
    const WindowsFilesystemProbe probe;

    QVERIFY(!probe.RecycleBinOn("Q:/nowhere").has_value());

    const TempFiles files;
    if (const std::optional<RecycleBinRoom> room = probe.RecycleBinOn(files.Root()))
    {
        QVERIFY(room->quota > 0);
    }
}

QTEST_MAIN(DeleteOnRealDiskTest)

#include "tst_delete_on_real_disk.moc"
