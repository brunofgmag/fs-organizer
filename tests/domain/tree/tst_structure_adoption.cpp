#include <QtTest/QtTest>

#include <algorithm>

#include "domain/model/CategoryMarker.h"
#include "domain/model/Manifest.h"
#include "domain/tree/StructureAdoption.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class StructureAdoptionTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryFolderTheUserBuiltIsDeclaredIncludingTheEmptyOnesAtAnyDepth();
        static void WhatTheAppBroughtInIsLeftAloneAndNeverLookedInside();
        static void AnAddonIsNotDeclaredAndNothingUnderItIsVisited();
        static void AFolderThatAlreadyCarriesTheMarkerIsNotOfferedAgain();
        static void TheMarkersAlreadyOnDiskAreAnsweredApartSoTheyCanBeTakenBack();
        static void TheImportersOwnLeftoversAndALinkAreLeftOut();
    };
}

namespace
{
    const std::filesystem::path kLibrary = "D:/MSFS 2024";

    void GiveItAManifest(InMemoryFileSystem& disk, const std::filesystem::path& folder)
    {
        disk.AddFileWithContents(ManifestPathIn(folder), R"({"title": "Whatever"})");
    }

    void AddFolderAndTheOnesAbove(InMemoryFileSystem& disk, const std::filesystem::path& folder)
    {
        for (std::filesystem::path walked = folder; walked != kLibrary && !walked.empty();
             walked = walked.parent_path())
        {
            disk.AddDirectory(walked);
        }
    }
}

void StructureAdoptionTest::EveryFolderTheUserBuiltIsDeclaredIncludingTheEmptyOnesAtAnyDepth()
{
    InMemoryFileSystem disk;
    GiveItAManifest(disk, kLibrary / "Aircrafts" / "pmdg-aircraft-738");
    AddFolderAndTheOnesAbove(disk, kLibrary / "Sceneries");
    AddFolderAndTheOnesAbove(disk, kLibrary / "Liveries" / "Brazil" / "Norte");

    const FakeFilesystemProbe probe(disk);
    const std::vector<std::filesystem::path> grouping = HowTheLibraryIsGrouped(probe, kLibrary, {}).notYetDeclared;

    QCOMPARE(grouping.size(), std::size_t{5});
    QVERIFY(std::ranges::find(grouping, kLibrary / "Aircrafts") != grouping.end());
    QVERIFY(std::ranges::find(grouping, kLibrary / "Sceneries") != grouping.end());
    QVERIFY(std::ranges::find(grouping, kLibrary / "Liveries") != grouping.end());
    QVERIFY(std::ranges::find(grouping, kLibrary / "Liveries" / "Brazil") != grouping.end());
    QVERIFY(std::ranges::find(grouping, kLibrary / "Liveries" / "Brazil" / "Norte") != grouping.end());
}

void StructureAdoptionTest::WhatTheAppBroughtInIsLeftAloneAndNeverLookedInside()
{
    InMemoryFileSystem disk;
    GiveItAManifest(disk, kLibrary / "Utils" / "navigraph-nav-base");
    AddFolderAndTheOnesAbove(disk, kLibrary / "Utils" / "ModelLib" / "CH47");

    const FakeFilesystemProbe probe(disk);
    const std::vector<std::filesystem::path> grouping =
        HowTheLibraryIsGrouped(probe, kLibrary, {kLibrary / "Utils" / "ModelLib"}).notYetDeclared;

    QCOMPARE(grouping.size(), std::size_t{1});
    QCOMPARE(grouping.front(), kLibrary / "Utils");
    QVERIFY2(!probe.WasEnumerated(kLibrary / "Utils" / "ModelLib"),
             "the walk looked inside a folder the app had brought in");
}

void StructureAdoptionTest::AnAddonIsNotDeclaredAndNothingUnderItIsVisited()
{
    InMemoryFileSystem disk;
    GiveItAManifest(disk, kLibrary / "Aircrafts" / "pmdg-aircraft-738");
    AddFolderAndTheOnesAbove(disk, kLibrary / "Aircrafts" / "pmdg-aircraft-738" / "SimObjects");

    const FakeFilesystemProbe probe(disk);
    const std::vector<std::filesystem::path> grouping = HowTheLibraryIsGrouped(probe, kLibrary, {}).notYetDeclared;

    QCOMPARE(grouping.size(), std::size_t{1});
    QCOMPARE(grouping.front(), kLibrary / "Aircrafts");
    QVERIFY2(!probe.WasEnumerated(kLibrary / "Aircrafts" / "pmdg-aircraft-738"), "the walk looked inside an addon");
}

void StructureAdoptionTest::AFolderThatAlreadyCarriesTheMarkerIsNotOfferedAgain()
{
    InMemoryFileSystem disk;
    disk.AddFile(CategoryMarkerPathIn(kLibrary / "Sceneries"));
    AddFolderAndTheOnesAbove(disk, kLibrary / "Sceneries" / "Brazil");

    const FakeFilesystemProbe probe(disk);
    const std::vector<std::filesystem::path> grouping = HowTheLibraryIsGrouped(probe, kLibrary, {}).notYetDeclared;

    QCOMPARE(grouping.size(), std::size_t{1});
    QCOMPARE(grouping.front(), kLibrary / "Sceneries" / "Brazil");
}

void StructureAdoptionTest::TheMarkersAlreadyOnDiskAreAnsweredApartSoTheyCanBeTakenBack()
{
    InMemoryFileSystem disk;
    disk.AddFile(CategoryMarkerPathIn(kLibrary / "Sceneries"));
    disk.AddFile(CategoryMarkerPathIn(kLibrary / "Sceneries" / "Brazil"));
    AddFolderAndTheOnesAbove(disk, kLibrary / "Liveries");

    const FakeFilesystemProbe probe(disk);
    const LibraryGrouping grouping = HowTheLibraryIsGrouped(probe, kLibrary, {});

    QCOMPARE(grouping.alreadyDeclared.size(), std::size_t{2});
    QVERIFY(std::ranges::find(grouping.alreadyDeclared, kLibrary / "Sceneries") != grouping.alreadyDeclared.end());
    QVERIFY(std::ranges::find(grouping.alreadyDeclared, kLibrary / "Sceneries" / "Brazil")
            != grouping.alreadyDeclared.end());

    QCOMPARE(grouping.notYetDeclared.size(), std::size_t{1});
    QCOMPARE(grouping.notYetDeclared.front(), kLibrary / "Liveries");
}

void StructureAdoptionTest::TheImportersOwnLeftoversAndALinkAreLeftOut()
{
    InMemoryFileSystem disk;
    AddFolderAndTheOnesAbove(disk, kLibrary / "_fsorganizer-quarantine" / "asfs");
    AddFolderAndTheOnesAbove(disk, kLibrary / "Utils" / "zGFX-FXLIB.fsorg-partial");
    disk.AddLink(kLibrary / "Utils" / "gsx-pro", "C:/Program Files (x86)/Addon Manager/MSFS/gsx-pro");

    const FakeFilesystemProbe probe(disk);
    const std::vector<std::filesystem::path> grouping = HowTheLibraryIsGrouped(probe, kLibrary, {}).notYetDeclared;

    QCOMPARE(grouping.size(), std::size_t{1});
    QCOMPARE(grouping.front(), kLibrary / "Utils");
}

QTEST_APPLESS_MAIN(StructureAdoptionTest)

#include "tst_structure_adoption.moc"
