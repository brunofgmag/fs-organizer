#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <sys/stat.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

#include "infrastructure/fileops/PosixFileOperations.h"
#include "support/PathText.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class PosixFileOperationsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void CopyingATreeReproducesEveryFileItsBytesItsModeAndItsWriteTime();
        static void CopyingReportsProgressInStepsThatEndAtTheTotal();
        static void CancellingTheProgressCallbackStopsTheCopyAndLeavesNoHalfFile();
        static void CopyingASourceThatCannotBeWalkedFailsInsteadOfLandingNothing();
        static void CopyingOverAFileThatIsThereFailsAndLeavesItAlone();
        static void CreatingAFolderThatIsAlreadyThereIsRefusedWhateverItIs();
        static void CreatingAFolderOpensTheWholeWayThere();
        static void TheHiddenFileIsCreatedEmptyAndTruncatedWhenItIsAlreadyThere();
        static void MovingIntoAFolderThatDoesNotExistYetOpensTheWayThere();
        static void MovingOntoSomethingThatIsThereIsRefusedAndLosesNothing();
        static void MovingAcrossVolumesIsRefusedInsteadOfCopied();
        static void RemovingATreeAtALinkNeverReachesTheTarget();
        static void RemovingATreeTakesTheWholeSubtreeAndAFileAlone();
        static void RemovingWhatIsNotThereFails();
        static void OnlyAnEmptyPhysicalFolderIsRemovedAsEmpty();
        static void RecyclingPutsTheFolderInTheTrashAndTakesItOutOfThePlace();
        static void RecyclingWhatIsNotThereFails();
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
    };

    std::string BytesOf(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);

        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    bool NothingIsAt(const std::filesystem::path& path)
    {
        return !std::filesystem::exists(std::filesystem::symlink_status(path));
    }

    std::size_t FilesUnder(const std::filesystem::path& root)
    {
        std::size_t files = 0;
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root))
        {
            files += entry.is_regular_file() ? 1 : 0;
        }

        return files;
    }

    const std::function<bool(const CopyProgress&)> kSilently = [](const CopyProgress&)
    {
        return true;
    };
}

void PosixFileOperationsTest::CopyingATreeReproducesEveryFileItsBytesItsModeAndItsWriteTime()
{
    const Disk disk;
    const std::filesystem::path source = disk.AddFolder("library/fenix");
    const std::string big(3 * 1024 * 1024 + 17, 'k');
    static_cast<void>(disk.AddFile("library/fenix/aircraft.cfg", "title = A320"));
    const std::filesystem::path bigFile = disk.AddFile("library/fenix/SimObjects/deep/model.bin", big);
    const std::filesystem::path script = disk.AddFile("library/fenix/run.sh", "#!/bin/sh");
    std::filesystem::permissions(script, std::filesystem::perms::owner_all | std::filesystem::perms::group_read,
                                 std::filesystem::perm_options::replace);
    const std::filesystem::file_time_type moved =
        std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * 90);
    std::filesystem::last_write_time(bigFile, moved);

    PosixFileOperations operations;
    const std::filesystem::path destination = disk.Root() / "Community" / "fenix";

    QCOMPARE(operations.CopyTree(source, destination, kSilently), CopyOutcome::Completed);

    QCOMPARE(FilesUnder(destination), std::size_t{3});
    QCOMPARE(BytesOf(destination / "aircraft.cfg"), std::string("title = A320"));
    QVERIFY(BytesOf(destination / "SimObjects" / "deep" / "model.bin") == big);
    QCOMPARE(std::filesystem::status(destination / "run.sh").permissions(),
             std::filesystem::status(script).permissions());
    QCOMPARE(std::filesystem::last_write_time(destination / "SimObjects" / "deep" / "model.bin"),
             std::filesystem::last_write_time(bigFile));
}

void PosixFileOperationsTest::CopyingReportsProgressInStepsThatEndAtTheTotal()
{
    const Disk disk;
    const std::filesystem::path source = disk.AddFolder("library/fenix");
    static_cast<void>(disk.AddFile("library/fenix/a.bin", std::string(2 * 1024 * 1024 + 5, 'a')));
    static_cast<void>(disk.AddFile("library/fenix/b.bin", std::string(1024 * 1024 + 9, 'b')));
    const std::uintmax_t total = 2 * 1024 * 1024 + 5 + 1024 * 1024 + 9;

    std::vector<CopyProgress> seen;
    PosixFileOperations operations;

    QCOMPARE(operations.CopyTree(source, disk.Root() / "Community" / "fenix",
                                 [&seen](const CopyProgress& progress)
                                 {
                                     seen.push_back(progress);

                                     return true;
                                 }),
             CopyOutcome::Completed);

    QVERIFY(seen.size() >= 4);
    for (std::size_t at = 0; at < seen.size(); ++at)
    {
        QCOMPARE(seen[at].totalBytes, total);
        QVERIFY(at == 0 || seen[at].copiedBytes > seen[at - 1].copiedBytes);
    }

    QCOMPARE(seen.back().copiedBytes, total);
}

void PosixFileOperationsTest::CancellingTheProgressCallbackStopsTheCopyAndLeavesNoHalfFile()
{
    const Disk disk;
    const std::filesystem::path source = disk.AddFolder("library/fenix");
    static_cast<void>(disk.AddFile("library/fenix/a.bin", std::string(4 * 1024 * 1024, 'a')));
    const std::filesystem::path destination = disk.Root() / "Community" / "fenix";

    int calls = 0;
    PosixFileOperations operations;

    QCOMPARE(operations.CopyTree(source, destination,
                                 [&calls](const CopyProgress&)
                                 {
                                     return ++calls < 2;
                                 }),
             CopyOutcome::Cancelled);

    QCOMPARE(calls, 2);
    QVERIFY(NothingIsAt(destination / "a.bin"));
}

void PosixFileOperationsTest::CopyingASourceThatCannotBeWalkedFailsInsteadOfLandingNothing()
{
    const Disk disk;
    const std::filesystem::path destination = disk.Root() / "Community" / "fenix";

    PosixFileOperations operations;

    QCOMPARE(operations.CopyTree(disk.Root() / "never-there", destination, kSilently), CopyOutcome::Failed);
    QVERIFY(NothingIsAt(destination));
}

void PosixFileOperationsTest::CopyingOverAFileThatIsThereFailsAndLeavesItAlone()
{
    const Disk disk;
    const std::filesystem::path source = disk.AddFolder("library/fenix");
    static_cast<void>(disk.AddFile("library/fenix/aircraft.cfg", "new"));
    const std::filesystem::path destination = disk.AddFolder("Community/fenix");
    static_cast<void>(disk.AddFile("Community/fenix/aircraft.cfg", "old"));

    PosixFileOperations operations;

    QCOMPARE(operations.CopyTree(source, destination, kSilently), CopyOutcome::Failed);
    QCOMPARE(BytesOf(destination / "aircraft.cfg"), std::string("old"));
}

void PosixFileOperationsTest::CreatingAFolderThatIsAlreadyThereIsRefusedWhateverItIs()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("library");
    const std::filesystem::path file = disk.AddFile("a-file", "x");
    const std::filesystem::path link = disk.Root() / "a-link";
    std::filesystem::create_directory_symlink(folder, link);
    std::filesystem::create_directory_symlink(disk.Root() / "never-there", disk.Root() / "dangling");

    PosixFileOperations operations;

    QVERIFY(!operations.CreateFolder(folder));
    QVERIFY(!operations.CreateFolder(file));
    QVERIFY(!operations.CreateFolder(link));
    QVERIFY(!operations.CreateFolder(disk.Root() / "dangling"));
}

void PosixFileOperationsTest::CreatingAFolderOpensTheWholeWayThere()
{
    const Disk disk;
    const std::filesystem::path deep = disk.Root() / "a" / "b" / "c";

    PosixFileOperations operations;

    QVERIFY(operations.CreateFolder(deep));
    QVERIFY(std::filesystem::is_directory(deep));
}

void PosixFileOperationsTest::TheHiddenFileIsCreatedEmptyAndTruncatedWhenItIsAlreadyThere()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("library");
    const std::filesystem::path marker = folder / ".fsorg-category";

    PosixFileOperations operations;

    QVERIFY(operations.WriteHiddenFile(marker));
    QCOMPARE(std::filesystem::file_size(marker), std::uintmax_t{0});

    static_cast<void>(disk.AddFile("library/.fsorg-category", "left over"));

    QVERIFY(operations.WriteHiddenFile(marker));
    QCOMPARE(std::filesystem::file_size(marker), std::uintmax_t{0});
    QVERIFY(!operations.WriteHiddenFile(disk.Root() / "never-there" / ".fsorg-category"));
}

void PosixFileOperationsTest::MovingIntoAFolderThatDoesNotExistYetOpensTheWayThere()
{
    const Disk disk;
    const std::filesystem::path source = disk.AddFolder("staging/fenix");
    static_cast<void>(disk.AddFile("staging/fenix/aircraft.cfg", "x"));
    const std::filesystem::path destination = disk.Root() / "library" / "Aircrafts" / "fenix";

    PosixFileOperations operations;

    QVERIFY(operations.Move(source, destination));
    QVERIFY(NothingIsAt(source));
    QCOMPARE(BytesOf(destination / "aircraft.cfg"), std::string("x"));
}

void PosixFileOperationsTest::MovingOntoSomethingThatIsThereIsRefusedAndLosesNothing()
{
    const Disk disk;
    const std::filesystem::path source = disk.AddFolder("staging/fenix");
    static_cast<void>(disk.AddFile("staging/fenix/aircraft.cfg", "new"));
    const std::filesystem::path emptyFolder = disk.AddFolder("library/empty");
    const std::filesystem::path file = disk.AddFile("library/file.txt", "old");

    PosixFileOperations operations;

    QVERIFY(!operations.Move(source, emptyFolder));
    QVERIFY(!operations.Move(source, file));
    QVERIFY(!operations.Move(disk.AddFile("staging/one.txt", "one"), file));

    QCOMPARE(BytesOf(source / "aircraft.cfg"), std::string("new"));
    QVERIFY(std::filesystem::is_empty(emptyFolder));
    QCOMPARE(BytesOf(file), std::string("old"));
    QCOMPARE(BytesOf(disk.Root() / "staging" / "one.txt"), std::string("one"));
}

void PosixFileOperationsTest::MovingAcrossVolumesIsRefusedInsteadOfCopied()
{
    const Disk disk;
    const QTemporaryDir other(QStringLiteral("/dev/shm/fsorg-move-XXXXXX"));
    const std::filesystem::path source = disk.AddFolder("staging/fenix");

    struct stat here
    {
    };

    struct stat there
    {
    };

    QCOMPARE(::stat(disk.Root().c_str(), &here), 0);

    const bool anotherVolumeIsAround =
        other.isValid() && ::stat(other.path().toUtf8().constData(), &there) == 0 && there.st_dev != here.st_dev;
    qInfo() << (anotherVolumeIsAround ? "/dev/shm is another volume: the move must be refused"
                                      : "no other volume to try: the move stays on one volume and must land");

    PosixFileOperations operations;
    const std::filesystem::path destination = (anotherVolumeIsAround ? AsPath(other.path()) : disk.Root()) / "fenix";

    QCOMPARE(operations.Move(source, destination), !anotherVolumeIsAround);
    QCOMPARE(NothingIsAt(source), !anotherVolumeIsAround);
    QCOMPARE(NothingIsAt(destination), anotherVolumeIsAround);
}

void PosixFileOperationsTest::RemovingATreeAtALinkNeverReachesTheTarget()
{
    const Disk disk;
    const std::filesystem::path target = disk.AddFolder("library/fenix");
    static_cast<void>(disk.AddFile("library/fenix/aircraft.cfg", "x"));
    const std::filesystem::path link = disk.AddFolder("Community").parent_path() / "Community" / "fenix";
    std::filesystem::create_directory_symlink(target, link);

    PosixFileOperations operations;

    QVERIFY(operations.RemoveTree(link));
    QVERIFY(NothingIsAt(link));
    QCOMPARE(BytesOf(target / "aircraft.cfg"), std::string("x"));
}

void PosixFileOperationsTest::RemovingATreeTakesTheWholeSubtreeAndAFileAlone()
{
    const Disk disk;
    const std::filesystem::path tree = disk.AddFolder("library/fenix");
    static_cast<void>(disk.AddFile("library/fenix/a/b/c/deep.cfg", "x"));
    static_cast<void>(disk.AddFile("library/fenix/top.cfg", "x"));
    const std::filesystem::path file = disk.AddFile("library/readme.txt", "x");
    std::filesystem::permissions(file, std::filesystem::perms::owner_read, std::filesystem::perm_options::replace);

    PosixFileOperations operations;

    QVERIFY(operations.RemoveTree(tree));
    QVERIFY(NothingIsAt(tree));
    QVERIFY(operations.RemoveTree(file));
    QVERIFY(NothingIsAt(file));
    QVERIFY(std::filesystem::is_directory(disk.Root() / "library"));
}

void PosixFileOperationsTest::RemovingWhatIsNotThereFails()
{
    const Disk disk;

    PosixFileOperations operations;

    QVERIFY(!operations.RemoveTree(disk.Root() / "never-there"));
}

void PosixFileOperationsTest::OnlyAnEmptyPhysicalFolderIsRemovedAsEmpty()
{
    const Disk disk;
    const std::filesystem::path empty = disk.AddFolder("empty");
    const std::filesystem::path full = disk.AddFolder("full");
    static_cast<void>(disk.AddFile("full/x.txt", "x"));
    const std::filesystem::path file = disk.AddFile("file.txt", "x");
    const std::filesystem::path link = disk.Root() / "link-to-empty";
    std::filesystem::create_directory_symlink(empty, link);

    PosixFileOperations operations;

    QVERIFY(!operations.RemoveEmptyFolder(link));
    QVERIFY(!operations.RemoveEmptyFolder(full));
    QVERIFY(!operations.RemoveEmptyFolder(file));
    QVERIFY(!operations.RemoveEmptyFolder(disk.Root() / "never-there"));
    QVERIFY(std::filesystem::is_directory(empty));
    QVERIFY(std::filesystem::is_symlink(link));

    QVERIFY(operations.RemoveEmptyFolder(empty));
    QVERIFY(NothingIsAt(empty));
}

void PosixFileOperationsTest::RecyclingPutsTheFolderInTheTrashAndTakesItOutOfThePlace()
{
    const Disk disk;
    const QByteArray kept = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", disk.directory.filePath(QStringLiteral("data")).toUtf8());
    static_cast<void>(disk.AddFolder("data"));

    const std::filesystem::path folder = disk.AddFolder("library/fenix");
    static_cast<void>(disk.AddFile("library/fenix/aircraft.cfg", "x"));

    PosixFileOperations operations;
    const bool recycled = operations.Recycle(folder);

    if (kept.isNull())
    {
        qunsetenv("XDG_DATA_HOME");
    }
    else
    {
        qputenv("XDG_DATA_HOME", kept);
    }

    QVERIFY(recycled);
    QVERIFY(NothingIsAt(folder));
    QVERIFY(std::filesystem::is_regular_file(disk.Root() / "data" / "Trash" / "files" / "fenix" / "aircraft.cfg"));
    QVERIFY(std::filesystem::is_regular_file(disk.Root() / "data" / "Trash" / "info" / "fenix.trashinfo"));
}

void PosixFileOperationsTest::RecyclingWhatIsNotThereFails()
{
    const Disk disk;

    PosixFileOperations operations;

    QVERIFY(!operations.Recycle(disk.Root() / "never-there"));
}

QTEST_APPLESS_MAIN(PosixFileOperationsTest)

#include "tst_posix_file_operations.moc"
