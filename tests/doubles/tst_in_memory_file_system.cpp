#include <QtTest/QtTest>

#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/PathPrinting.h"

class InMemoryFileSystemTest : public QObject
{
    Q_OBJECT

private slots:
    static void RemovingANodeLeavesTheRestOfTheTreeStanding();
    static void ADirectoryThatStillHasContentCannotBeRemovedAsASingleNode();
    static void RemovingATreeTakesEveryDescendantWithIt();
    static void RemovingATreeAtALinkNeverReachesTheTarget();
    static void ASiblingWholeNameStartsWithTheRootIsNotADescendant();
    static void FilesUnderReachesEveryDepthAndReportsSizes();
    static void ChildDirectoriesReportsLinksButNeverFiles();
};

namespace
{
    InMemoryFileSystem AddonOnDiskAndLinkedIntoTheSimulator()
    {
        InMemoryFileSystem fileSystem;
        fileSystem.AddDirectory("D:/Library/Aircrafts/aerosoft-crj");
        fileSystem.AddFile("D:/Library/Aircrafts/aerosoft-crj/manifest.json", 512);
        fileSystem.AddDirectory("D:/Library/Aircrafts/aerosoft-crj/SimObjects");
        fileSystem.AddFile("D:/Library/Aircrafts/aerosoft-crj/SimObjects/model.gltf", 4096);
        fileSystem.AddDirectory("E:/Sim/Community");
        fileSystem.AddLink("E:/Sim/Community/aerosoft-crj", "D:/Library/Aircrafts/aerosoft-crj");
        return fileSystem;
    }
}

void InMemoryFileSystemTest::RemovingANodeLeavesTheRestOfTheTreeStanding()
{
    InMemoryFileSystem fileSystem = AddonOnDiskAndLinkedIntoTheSimulator();

    QVERIFY(fileSystem.RemoveNode("E:/Sim/Community/aerosoft-crj"));

    QVERIFY(!fileSystem.Exists("E:/Sim/Community/aerosoft-crj"));
    QVERIFY(fileSystem.Exists("E:/Sim/Community"));
    QVERIFY(fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj/SimObjects/model.gltf"));
}

void InMemoryFileSystemTest::ADirectoryThatStillHasContentCannotBeRemovedAsASingleNode()
{
    InMemoryFileSystem fileSystem = AddonOnDiskAndLinkedIntoTheSimulator();

    QVERIFY(!fileSystem.RemoveNode("D:/Library/Aircrafts/aerosoft-crj"));

    QVERIFY(fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj"));
    QVERIFY(fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj/manifest.json"));
}

void InMemoryFileSystemTest::RemovingATreeTakesEveryDescendantWithIt()
{
    InMemoryFileSystem fileSystem = AddonOnDiskAndLinkedIntoTheSimulator();

    QVERIFY(fileSystem.RemoveTree("D:/Library/Aircrafts/aerosoft-crj"));

    QVERIFY(!fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj"));
    QVERIFY(!fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj/manifest.json"));
    QVERIFY(!fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj/SimObjects"));
    QVERIFY(!fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj/SimObjects/model.gltf"));
    QVERIFY(fileSystem.Exists("E:/Sim/Community/aerosoft-crj"));
}

void InMemoryFileSystemTest::RemovingATreeAtALinkNeverReachesTheTarget()
{
    InMemoryFileSystem fileSystem = AddonOnDiskAndLinkedIntoTheSimulator();

    QVERIFY(fileSystem.RemoveTree("E:/Sim/Community/aerosoft-crj"));

    QVERIFY(!fileSystem.Exists("E:/Sim/Community/aerosoft-crj"));
    QVERIFY(fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj"));
    QVERIFY(fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj/manifest.json"));
    QVERIFY(fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj/SimObjects/model.gltf"));
}

void InMemoryFileSystemTest::ASiblingWholeNameStartsWithTheRootIsNotADescendant()
{
    InMemoryFileSystem fileSystem;
    fileSystem.AddDirectory("D:/Lib");
    fileSystem.AddFile("D:/Lib/keep.txt");
    fileSystem.AddDirectory("D:/Library");
    fileSystem.AddFile("D:/Library/keep.txt");

    QVERIFY(fileSystem.RemoveTree("D:/Lib"));

    QVERIFY(!fileSystem.Exists("D:/Lib/keep.txt"));
    QVERIFY(fileSystem.Exists("D:/Library"));
    QVERIFY(fileSystem.Exists("D:/Library/keep.txt"));
}

void InMemoryFileSystemTest::FilesUnderReachesEveryDepthAndReportsSizes()
{
    const InMemoryFileSystem fileSystem = AddonOnDiskAndLinkedIntoTheSimulator();

    const std::vector<std::filesystem::path> files = fileSystem.FilesUnder("D:/Library/Aircrafts/aerosoft-crj");

    QCOMPARE(files,
             (std::vector<std::filesystem::path>{"D:/Library/Aircrafts/aerosoft-crj/SimObjects/model.gltf",
                                                 "D:/Library/Aircrafts/aerosoft-crj/manifest.json"}));
    QCOMPARE(fileSystem.FileSize("D:/Library/Aircrafts/aerosoft-crj/SimObjects/model.gltf"), std::uintmax_t{4096});
}

void InMemoryFileSystemTest::ChildDirectoriesReportsLinksButNeverFiles()
{
    InMemoryFileSystem fileSystem = AddonOnDiskAndLinkedIntoTheSimulator();
    fileSystem.AddFile("E:/Sim/Community/loose.txt");
    fileSystem.AddDirectory("E:/Sim/Community/asfs");

    const std::vector<std::filesystem::path> children = fileSystem.ChildDirectoriesOf("E:/Sim/Community");

    QCOMPARE(children, (std::vector<std::filesystem::path>{"E:/Sim/Community/aerosoft-crj", "E:/Sim/Community/asfs"}));
}

QTEST_APPLESS_MAIN(InMemoryFileSystemTest)

#include "tst_in_memory_file_system.moc"
