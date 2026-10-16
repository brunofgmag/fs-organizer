#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "domain/support/PathUtils.h"
#include "infrastructure/fileops/PosixSidecarStore.h"
#include "support/PathText.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class PosixSidecarStoreTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void FilesInListsOnlyTheFilesHeldDirectlyInTheFolderWithTheirOnDiskCase();
        static void FilesInAFolderThatIsNotThereIsEmpty();
        static void AWrittenFileReadsBackByteForByteAndIsListed();
        static void WritingAgainReplacesTheWholeContents();
        static void WritingIntoAFolderThatIsNotThereFailsAndCreatesNothing();
        static void ReadingWhatIsNotThereOrIsAFolderAnswersNothing();
        static void ForgettingTakesTheFileOutAndAnswersFalseWhenThereWasNone();
    };

    void Touch(const std::filesystem::path& path)
    {
        std::ofstream file(path, std::ios::binary);
        file << "x";
    }

    std::set<std::string> NamesOf(const std::vector<std::filesystem::path>& files)
    {
        std::set<std::string> names;
        for (const std::filesystem::path& file : files)
        {
            names.insert(AsUtf8(file.filename()));
        }

        return names;
    }
}

void PosixSidecarStoreTest::FilesInListsOnlyTheFilesHeldDirectlyInTheFolderWithTheirOnDiskCase()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path folder = AsPath(directory.path()) / "Aircrafts";

    QVERIFY(std::filesystem::create_directories(folder / "Nested"));
    Touch(folder / "Aerosoft-CRJ.FSORG-External");
    Touch(folder / "notes.txt");
    Touch(folder / "Nested" / "deeper.fsorg-external");
    QVERIFY(std::filesystem::create_directories(folder / "folder.fsorg-external"));

    const PosixSidecarStore store;
    const std::vector<std::filesystem::path> files = store.FilesIn(folder);

    QCOMPARE(NamesOf(files), (std::set<std::string>{"Aerosoft-CRJ.FSORG-External", "notes.txt"}));
    QVERIFY(std::ranges::all_of(files,
                                [&folder](const std::filesystem::path& file)
                                {
                                    return file.parent_path() == folder;
                                }));
}

void PosixSidecarStoreTest::FilesInAFolderThatIsNotThereIsEmpty()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const PosixSidecarStore store;

    QVERIFY(store.FilesIn(AsPath(directory.path()) / "missing").empty());
}

void PosixSidecarStoreTest::AWrittenFileReadsBackByteForByteAndIsListed()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path folder = AsPath(directory.path());
    const std::filesystem::path sidecar = folder / "fenix-a320.fsorg-external";
    const std::string bytes = std::string("D:/Other/fenix\r\n\0\xFF end", 22);

    PosixSidecarStore store;
    QVERIFY(store.Write(sidecar, bytes));

    const std::vector<std::filesystem::path> files = store.FilesIn(folder);

    QCOMPARE(files.size(), std::size_t{1});
    QCOMPARE(files.front(), sidecar);
    QCOMPARE(store.Read(sidecar), std::optional(bytes));
}

void PosixSidecarStoreTest::WritingAgainReplacesTheWholeContents()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path sidecar = AsPath(directory.path()) / "origin.fsorg-origin";

    PosixSidecarStore store;
    QVERIFY(store.Write(sidecar, "a much longer first version"));
    QVERIFY(store.Write(sidecar, "short"));

    QCOMPARE(store.Read(sidecar), std::optional<std::string>("short"));
}

void PosixSidecarStoreTest::WritingIntoAFolderThatIsNotThereFailsAndCreatesNothing()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path folder = AsPath(directory.path()) / "missing";

    PosixSidecarStore store;

    QVERIFY(!store.Write(folder / "origin.fsorg-origin", "text"));
    QVERIFY(!std::filesystem::exists(folder));
}

void PosixSidecarStoreTest::ReadingWhatIsNotThereOrIsAFolderAnswersNothing()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path folder = AsPath(directory.path());
    QVERIFY(std::filesystem::create_directories(folder / "a-folder"));

    const PosixSidecarStore store;

    QVERIFY(!store.Read(folder / "missing").has_value());
    QVERIFY(!store.Read(folder / "a-folder").has_value());
}

void PosixSidecarStoreTest::ForgettingTakesTheFileOutAndAnswersFalseWhenThereWasNone()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path sidecar = AsPath(directory.path()) / "origin.fsorg-origin";

    PosixSidecarStore store;
    QVERIFY(store.Write(sidecar, "text"));

    QVERIFY(store.Forget(sidecar));
    QVERIFY(!std::filesystem::exists(sidecar));
    QVERIFY(!store.Forget(sidecar));
}

QTEST_APPLESS_MAIN(PosixSidecarStoreTest)

#include "tst_posix_sidecar_store.moc"
