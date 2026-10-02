#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "domain/importing/ExternalSidecar.h"
#include "domain/support/PathUtils.h"
#include "infrastructure/fileops/WindowsSidecarStore.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class SidecarStoreOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void FilesInListsOnlyTheFilesHeldDirectlyInTheFolderWithTheirOnDiskCase();
        static void FilesInAFolderThatIsNotThereIsEmpty();
        static void AFileWrittenByTheStoreIsListedAndReadBack();
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

void SidecarStoreOnRealDiskTest::FilesInListsOnlyTheFilesHeldDirectlyInTheFolderWithTheirOnDiskCase()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path folder = std::filesystem::path(directory.path().toStdWString()) / "Aircrafts";

    QVERIFY(std::filesystem::create_directories(folder / "Nested"));
    Touch(folder / "Aerosoft-CRJ.FSORG-External");
    Touch(folder / "notes.txt");
    Touch(folder / "Nested" / "deeper.fsorg-external");
    QVERIFY(std::filesystem::create_directories(folder / "folder.fsorg-external"));

    const WindowsSidecarStore store;
    const std::vector<std::filesystem::path> files = store.FilesIn(folder);

    QCOMPARE(NamesOf(files), (std::set<std::string>{"Aerosoft-CRJ.FSORG-External", "notes.txt"}));
    QVERIFY(std::ranges::all_of(files,
                                [&folder](const std::filesystem::path& file)
                                {
                                    return file.parent_path() == folder;
                                }));
}

void SidecarStoreOnRealDiskTest::FilesInAFolderThatIsNotThereIsEmpty()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const WindowsSidecarStore store;

    QVERIFY(store.FilesIn(std::filesystem::path(directory.path().toStdWString()) / "missing").empty());
}

void SidecarStoreOnRealDiskTest::AFileWrittenByTheStoreIsListedAndReadBack()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path folder = std::filesystem::path(directory.path().toStdWString());
    const std::filesystem::path sidecar = ExternalSidecarPathFor(folder / "fenix-a320");

    WindowsSidecarStore store;
    QVERIFY(store.Write(sidecar, TextOfTheExternalOrigin("C:/Other/fenix")));

    const std::vector<std::filesystem::path> files = store.FilesIn(folder);

    QCOMPARE(files.size(), std::size_t{1});
    QCOMPARE(ComparablePath(files.front()), ComparablePath(sidecar));
    QCOMPARE(ExternalOriginFromText(store.Read(files.front()).value_or("")).value_or(std::filesystem::path{}),
             std::filesystem::path{"C:/Other/fenix"});

    QVERIFY(store.Forget(sidecar));
    QVERIFY(store.FilesIn(folder).empty());
}

QTEST_APPLESS_MAIN(SidecarStoreOnRealDiskTest)

#include "tst_sidecar_store_on_real_disk.moc"
