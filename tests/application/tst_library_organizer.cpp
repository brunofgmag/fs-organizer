#include <QtTest/QtTest>

#include <variant>

#include "domain/journal/OperationLog.h"
#include "application/LibraryOrganizer.h"
#include "domain/tree/LibraryLookup.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

class LibraryOrganizerTest : public QObject
{
    Q_OBJECT

private slots:
    static void MovingAnEnabledAddonRecreatesTheLinkPointingAtTheNewPath();
    static void MovingAnAddonDoesNotChangeItsIdentity();
    static void MovingAnAddonToACategoryWithItsOwnDestinationRelinksThere();
    static void MovingADisabledAddonLeavesTheDestinationAlone();
    static void MovingIsRefusedWhenTheNameIsAlreadyTakenInTheLibrary();
    static void NoFolderIsMovedWhileTheSimulatorIsRunning();
    static void EveryStepOfAMoveIsJournalled();
    static void ACreatedCategoryIsAFolderInTheLibrary();
    static void ACategoryIsNotCreatedOutsideALibrary();
    static void RenamingACategoryCarriesItsEnabledAddonsAlong();
};

namespace
{
    constexpr std::uintmax_t kMegabyte = 1024 * 1024;

    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kOtherDestination = "E:/Sim/Community2024";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kAircrafts = "D:/Library/Aircrafts";
    const std::filesystem::path kAircrafts2024 = "D:/Library/Aircrafts (2024)";
    const std::filesystem::path kAddon = "D:/Library/Aircrafts/aerosoft-crj";
    const std::filesystem::path kMoved = "D:/Library/Aircrafts (2024)/aerosoft-crj";
    const std::filesystem::path kLink = "E:/Sim/Community/aerosoft-crj";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{path, Manifest{}};

        return node;
    }

    struct Fixture
    {
        InMemoryFileSystem fileSystem;
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeLinkService linkService{fileSystem};
        FakeProcessProbe processProbe;
        EntryClassifier classifier{linkService, filesystemProbe};
        LinkingEngine linking{linkService, filesystemProbe};
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeCatalogScanner catalog;
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};

        SimulatorProfile profile{.destinations = {kDestination, kOtherDestination},
                                 .defaultDestination = kDestination,
                                 .libraries = {Library{"lib-1", kLibrary}}};

        Fixture()
        {
            fileSystem.AddDirectory(kDestination);
            fileSystem.AddDirectory(kOtherDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kAircrafts);
            fileSystem.AddDirectory(kAircrafts2024);
            fileSystem.AddDirectory(kAddon);
            fileSystem.AddFile(kAddon / "manifest.json", kMegabyte);

            TheLibraryHolds({kAddon});
        }

        void TheLibraryHolds(const std::vector<std::filesystem::path>& addons)
        {
            TreeNode library;
            library.kind = TreeNodeKind::Library;
            library.path = kLibrary;

            for (const std::filesystem::path& addon : addons)
            {
                library.children.push_back(AddonNode(addon));
            }

            catalog.SetTree(kLibrary, std::move(library));
        }

        void EnableTheAddonIn(const std::filesystem::path& destination)
        {
            fileSystem.AddLink(destination / kAddon.filename(), kAddon);
        }

        void GiveTheCategoryItsOwnDestination(const std::filesystem::path& category,
                                              const std::filesystem::path& destination)
        {
            profile.destinationOverrides.push_back(
                DestinationOverride{"lib-1", RelativeToLibrary(profile.libraries.front(), category), destination});
        }
    };
}

void LibraryOrganizerTest::MovingAnEnabledAddonRecreatesTheLinkPointingAtTheNewPath()
{
    Fixture f;
    f.EnableTheAddonIn(kDestination);

    const std::vector<FileOperationResult> results = f.organizer.Move(f.profile, {AddonMove{kAddon, kAircrafts2024}});

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, ImportResult::Completed);
    QVERIFY(f.fileSystem.Exists(kMoved / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kAddon));
    QVERIFY(f.fileSystem.IsLink(kLink));
    QCOMPARE(f.fileSystem.LinkTarget(kLink).value(), kMoved);
}

void LibraryOrganizerTest::MovingAnAddonDoesNotChangeItsIdentity()
{
    Fixture f;
    f.EnableTheAddonIn(kDestination);

    const AddonId before = IdentityOf(f.profile, kAddon);

    QCOMPARE(f.organizer.Move(f.profile, {AddonMove{kAddon, kAircrafts2024}}).front().result, ImportResult::Completed);

    const AddonId after = IdentityOf(f.profile, kMoved);

    QVERIFY(before == after);
    QCOMPARE(after.folderName, std::string{"aerosoft-crj"});
}

void LibraryOrganizerTest::MovingAnAddonToACategoryWithItsOwnDestinationRelinksThere()
{
    Fixture f;
    f.GiveTheCategoryItsOwnDestination(kAircrafts2024, kOtherDestination);
    f.EnableTheAddonIn(kDestination);

    QCOMPARE(f.organizer.Move(f.profile, {AddonMove{kAddon, kAircrafts2024}}).front().result, ImportResult::Completed);

    QVERIFY(!f.fileSystem.Exists(kLink));
    QVERIFY(f.fileSystem.IsLink(kOtherDestination / "aerosoft-crj"));
    QCOMPARE(f.fileSystem.LinkTarget(kOtherDestination / "aerosoft-crj").value(), kMoved);
}

void LibraryOrganizerTest::MovingADisabledAddonLeavesTheDestinationAlone()
{
    Fixture f;

    QCOMPARE(f.organizer.Move(f.profile, {AddonMove{kAddon, kAircrafts2024}}).front().result, ImportResult::Completed);

    QVERIFY(f.fileSystem.Exists(kMoved / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kLink));
    QVERIFY(!f.fileSystem.Exists(kOtherDestination / "aerosoft-crj"));
}

void LibraryOrganizerTest::MovingIsRefusedWhenTheNameIsAlreadyTakenInTheLibrary()
{
    Fixture f;
    f.fileSystem.AddDirectory(kMoved);
    f.fileSystem.AddFile(kMoved / "manifest.json", 2 * kMegabyte);
    f.TheLibraryHolds({kAddon, kMoved});
    f.EnableTheAddonIn(kDestination);

    const std::vector<FileOperationResult> results = f.organizer.Move(f.profile, {AddonMove{kAddon, kAircrafts2024}});

    QCOMPARE(results.front().result, ImportResult::TheIdentityIsTaken);
    QCOMPARE(results.front().occupant, kMoved);
    QVERIFY(f.fileSystem.Exists(kAddon / "manifest.json"));
    QCOMPARE(f.fileSystem.FileSize(kMoved / "manifest.json"), 2 * kMegabyte);
    QVERIFY(f.fileSystem.IsLink(kLink));
    QVERIFY(f.journal.appended.empty());
}

void LibraryOrganizerTest::NoFolderIsMovedWhileTheSimulatorIsRunning()
{
    Fixture f;
    f.EnableTheAddonIn(kDestination);
    f.processProbe.ReportTheSimulatorAsRunning();

    const std::vector<FileOperationResult> results = f.organizer.Move(f.profile, {AddonMove{kAddon, kAircrafts2024}});

    QCOMPARE(results.front().result, ImportResult::TheSimulatorIsRunning);
    QVERIFY(f.fileSystem.Exists(kAddon / "manifest.json"));
    QVERIFY(f.fileSystem.IsLink(kLink));
    QVERIFY(f.journal.appended.empty());

    QCOMPARE(f.organizer.CreateCategory(f.profile, kLibrary, "Sceneries").result, ImportResult::TheSimulatorIsRunning);
    QVERIFY(!f.fileSystem.Exists("D:/Library/Sceneries"));
}

void LibraryOrganizerTest::EveryStepOfAMoveIsJournalled()
{
    Fixture f;
    f.EnableTheAddonIn(kDestination);

    QCOMPARE(f.organizer.Move(f.profile, {AddonMove{kAddon, kAircrafts2024}}).front().result, ImportResult::Completed);

    QCOMPARE(f.journal.appended.size(), std::size_t{3});

    QCOMPARE(f.journal.appended[0].kind, OperationKind::DisableAddon);
    QCOMPARE(f.journal.appended[0].source, kLink);

    QCOMPARE(f.journal.appended[1].kind, OperationKind::MoveAddon);
    QCOMPARE(std::get<ImportResult>(f.journal.appended[1].outcome), ImportResult::Completed);
    QCOMPARE(f.journal.appended[1].source, kAddon);
    QCOMPARE(f.journal.appended[1].target, kMoved);
    QCOMPARE(f.journal.appended[1].addonId.folderName, std::string{"aerosoft-crj"});
    QCOMPARE(f.journal.appended[1].timestamp, f.clock.now);

    QCOMPARE(f.journal.appended[2].kind, OperationKind::EnableAddon);
    QCOMPARE(f.journal.appended[2].source, kMoved);
    QCOMPARE(f.journal.appended[2].target, kLink);
}

void LibraryOrganizerTest::ACreatedCategoryIsAFolderInTheLibrary()
{
    Fixture f;

    const FileOperationResult result = f.organizer.CreateCategory(f.profile, kLibrary, "Sceneries");

    QCOMPARE(result.result, ImportResult::Completed);
    QCOMPARE(result.path, std::filesystem::path{"D:/Library/Sceneries"});
    QVERIFY(f.fileSystem.IsDirectory("D:/Library/Sceneries"));
    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(f.journal.appended.front().kind, OperationKind::CreateCategory);
    QCOMPARE(f.journal.appended.front().target, std::filesystem::path{"D:/Library/Sceneries"});
}

void LibraryOrganizerTest::ACategoryIsNotCreatedOutsideALibrary()
{
    Fixture f;

    QCOMPARE(f.organizer.CreateCategory(f.profile, kDestination, "Sceneries").result,
             ImportResult::TheTargetIsNotInALibrary);
    QVERIFY(!f.fileSystem.Exists("E:/Sim/Community/Sceneries"));
    QVERIFY(f.journal.appended.empty());
}

void LibraryOrganizerTest::RenamingACategoryCarriesItsEnabledAddonsAlong()
{
    Fixture f;
    f.EnableTheAddonIn(kDestination);

    const FileOperationResult result = f.organizer.RenameCategory(f.profile, kAircrafts, "Airplanes");

    QCOMPARE(result.result, ImportResult::Completed);
    QVERIFY(f.fileSystem.Exists("D:/Library/Airplanes/aerosoft-crj/manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kAircrafts));
    QVERIFY(f.fileSystem.IsLink(kLink));
    QCOMPARE(f.fileSystem.LinkTarget(kLink).value(), std::filesystem::path{"D:/Library/Airplanes/aerosoft-crj"});
}

QTEST_APPLESS_MAIN(LibraryOrganizerTest)

#include "tst_library_organizer.moc"
