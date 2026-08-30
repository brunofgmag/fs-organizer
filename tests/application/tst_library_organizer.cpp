#include <QtTest/QtTest>

#include <string>
#include <variant>

#include "domain/support/PathSegment.h"

#include "domain/journal/OperationLog.h"
#include "application/LibraryOrganizer.h"
#include "domain/importing/ExternalSidecar.h"
#include "domain/model/CategoryMarker.h"
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

namespace
{
    class LibraryOrganizerTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void MovingAnEnabledAddonRecreatesTheLinkPointingAtTheNewPath();
        static void MovingAnAddonDoesNotChangeItsIdentity();
        static void MovingAnAddonToACategoryWithItsOwnDestinationRelinksThere();
        static void MovingADisabledAddonLeavesTheDestinationAlone();
        static void MovingAnImportedExternalAddonTakesTheRecordBesideItAlong();
        static void MovingIsRefusedWhenTheNameIsAlreadyTakenInTheLibrary();
        static void NoFolderIsMovedWhileTheSimulatorIsRunning();
        static void EveryStepOfAMoveIsJournalled();
        static void EmptyingACategoryLeavesItDeclaredSoItStaysAMoveTarget();
        static void AnAddonLeavingTheLibraryRootDoesNotDeclareTheRootACategory();
        static void ACreatedCategoryIsAFolderInTheLibrary();
        static void ACategoryTheMarkerCouldNotReachIsReportedInsteadOfPassingSilently();
        static void DeclaringAFolderThatIsNotOnDiskRefusesInsteadOfCreatingIt();
        static void ACategoryIsNotCreatedOutsideALibrary();
        static void ANameLongerThanAFolderNameSaysSoBeforeTheDiskIsTouched();
        static void AnEmptyCategoryIsRemovedAlongWithItsMarkerAndItsOverride();
        static void AStaleOverrideBelowTheRemovedCategoryIsForgottenToo();
        static void ACategoryThatStillHoldsAnAddonIsNotRemovedEvenWhenTheTreeSaysItIsEmpty();
        static void AFolderTheScanNeverSawIsRefusedInsteadOfRemovedOnFaith();
        static void TheLibraryRootIsNeverRemovedAsIfItWereACategory();
        static void NoCategoryIsRemovedWhileTheSimulatorIsRunning();
        static void RenamingACategoryCarriesItsEnabledAddonsAlong();
        static void AnOverrideCarriedByARenameKeepsTheSpellingItWasWrittenWith();
        static void ABatchOfMovesClassifiesTheDestinationsOncePerLibrary();
    };
}

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
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    TreeNode CategoryNode(const std::filesystem::path& path, std::vector<TreeNode> children)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Category;
        node.path = path;
        node.children = std::move(children);

        return node;
    }

    TreeNode DeclaredCategoryNode(const std::filesystem::path& path)
    {
        TreeNode node = CategoryNode(path, {});
        node.declaredAsCategory = true;

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
                                 .libraries = {Library{.id = "lib-1", .path = kLibrary}}};

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

        void TheLibraryIsMadeOf(std::vector<TreeNode> children)
        {
            TreeNode library;
            library.kind = TreeNodeKind::Library;
            library.path = kLibrary;
            library.children = std::move(children);

            catalog.SetTree(kLibrary, std::move(library));
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
                DestinationOverride{.libraryId = "lib-1",
                                    .relativePath = RelativeToLibrary(profile.libraries.front(), category),
                                    .destination = destination});
        }
    };
}

void LibraryOrganizerTest::MovingAnEnabledAddonRecreatesTheLinkPointingAtTheNewPath()
{
    Fixture f;
    f.EnableTheAddonIn(kDestination);

    const std::vector<FileOperationResult> results =
        f.organizer.Move(f.profile, {AddonMove{.addonFolder = kAddon, .category = kAircrafts2024}});

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::Completed);
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

    QCOMPARE(f.organizer.Move(f.profile, {AddonMove{kAddon, kAircrafts2024}}).front().result, FileResult::Completed);

    const AddonId after = IdentityOf(f.profile, kMoved);

    QVERIFY(before == after);
    QCOMPARE(after.folderName, std::string{"aerosoft-crj"});
}

void LibraryOrganizerTest::MovingAnAddonToACategoryWithItsOwnDestinationRelinksThere()
{
    Fixture f;
    f.GiveTheCategoryItsOwnDestination(kAircrafts2024, kOtherDestination);
    f.EnableTheAddonIn(kDestination);

    QCOMPARE(f.organizer.Move(f.profile, {AddonMove{kAddon, kAircrafts2024}}).front().result, FileResult::Completed);

    QVERIFY(!f.fileSystem.Exists(kLink));
    QVERIFY(f.fileSystem.IsLink(kOtherDestination / "aerosoft-crj"));
    QCOMPARE(f.fileSystem.LinkTarget(kOtherDestination / "aerosoft-crj").value(), kMoved);
}

void LibraryOrganizerTest::MovingADisabledAddonLeavesTheDestinationAlone()
{
    Fixture f;

    QCOMPARE(f.organizer.Move(f.profile, {AddonMove{kAddon, kAircrafts2024}}).front().result, FileResult::Completed);

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

    const std::vector<FileOperationResult> results =
        f.organizer.Move(f.profile, {AddonMove{.addonFolder = kAddon, .category = kAircrafts2024}});

    QCOMPARE(results.front().result, FileResult::TheIdentityIsTaken);
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

    const std::vector<FileOperationResult> results =
        f.organizer.Move(f.profile, {AddonMove{.addonFolder = kAddon, .category = kAircrafts2024}});

    QCOMPARE(results.front().result, FileResult::TheSimulatorIsRunning);
    QVERIFY(f.fileSystem.Exists(kAddon / "manifest.json"));
    QVERIFY(f.fileSystem.IsLink(kLink));
    QVERIFY(f.journal.appended.empty());

    QCOMPARE(f.organizer.CreateCategory(f.profile, kLibrary, "Sceneries").result, FileResult::TheSimulatorIsRunning);
    QVERIFY(!f.fileSystem.Exists("D:/Library/Sceneries"));
}

void LibraryOrganizerTest::EveryStepOfAMoveIsJournalled()
{
    Fixture f;
    f.EnableTheAddonIn(kDestination);

    QCOMPARE(f.organizer.Move(f.profile, {AddonMove{kAddon, kAircrafts2024}}).front().result, FileResult::Completed);

    QCOMPARE(f.journal.appended.size(), std::size_t{3});

    QCOMPARE(f.journal.appended[0].kind, OperationKind::DisableAddon);
    QCOMPARE(f.journal.appended[0].source, kAddon);
    QCOMPARE(f.journal.appended[0].target, kLink);

    QCOMPARE(f.journal.appended[1].kind, OperationKind::MoveAddon);
    QCOMPARE(std::get<FileResult>(f.journal.appended[1].outcome), FileResult::Completed);
    QCOMPARE(f.journal.appended[1].source, kAddon);
    QCOMPARE(f.journal.appended[1].target, kMoved);
    QCOMPARE(f.journal.appended[1].addonId.folderName, std::string{"aerosoft-crj"});
    QCOMPARE(f.journal.appended[1].timestamp, f.clock.now);

    QCOMPARE(f.journal.appended[2].kind, OperationKind::EnableAddon);
    QCOMPARE(f.journal.appended[2].source, kMoved);
    QCOMPARE(f.journal.appended[2].target, kLink);
}

void LibraryOrganizerTest::MovingAnImportedExternalAddonTakesTheRecordBesideItAlong()
{
    Fixture f;
    const std::filesystem::path came = "C:/Addon Manager/Aircraft/aerosoft-crj";
    f.fileSystem.AddFileWithContents(ExternalSidecarPathFor(kAddon), TextOfTheExternalOrigin(came));

    QCOMPARE(f.organizer.Move(f.profile, {AddonMove{kAddon, kAircrafts2024}}).front().result, FileResult::Completed);

    QVERIFY2(!f.fileSystem.Exists(ExternalSidecarPathFor(kAddon)),
             "the record stayed behind in the old category, where it will greet the next addon of the same name");
    QVERIFY2(f.fileSystem.Exists(ExternalSidecarPathFor(kMoved)),
             "the addon moved without the record that says which program it came from");
    QCOMPARE(f.fileSystem.ContentsOf(ExternalSidecarPathFor(kMoved)), TextOfTheExternalOrigin(came));
}

void LibraryOrganizerTest::ACreatedCategoryIsAFolderInTheLibrary()
{
    const Fixture f;

    const FileOperationResult result = f.organizer.CreateCategory(f.profile, kLibrary, "Sceneries");

    QCOMPARE(result.result, FileResult::Completed);
    QCOMPARE(result.path, std::filesystem::path{"D:/Library/Sceneries"});
    QVERIFY(f.fileSystem.IsDirectory("D:/Library/Sceneries"));
    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(f.journal.appended.front().kind, OperationKind::CreateCategory);
    QCOMPARE(f.journal.appended.front().target, std::filesystem::path{"D:/Library/Sceneries"});
    QVERIFY(f.fileSystem.Exists(CategoryMarkerPathIn("D:/Library/Sceneries")));
}

void LibraryOrganizerTest::ANameLongerThanAFolderNameSaysSoBeforeTheDiskIsTouched()
{
    const Fixture f;
    const std::string longest(kASegmentStopsAt, 'n');

    QCOMPARE(f.organizer.CreateCategory(f.profile, kLibrary, longest).result, FileResult::Completed);

    const std::string tooLong(kASegmentStopsAt + 1, 'n');
    const FileOperationResult refused = f.organizer.CreateCategory(f.profile, kLibrary, tooLong);

    QCOMPARE(refused.result, FileResult::ThePathIsTooLong);
    QVERIFY(!f.fileSystem.Exists(refused.path));
    QCOMPARE(f.journal.appended.size(), std::size_t{1});
}

void LibraryOrganizerTest::ACategoryTheMarkerCouldNotReachIsReportedInsteadOfPassingSilently()
{
    Fixture f;
    f.files.MakeTheHiddenWriteFail();

    const FileOperationResult result = f.organizer.CreateCategory(f.profile, kLibrary, "Sceneries");

    QCOMPARE(result.result, FileResult::CouldNotCreateTheCategory);
    QCOMPARE(std::get<FileResult>(f.journal.appended.front().outcome), FileResult::CouldNotCreateTheCategory);
}

void LibraryOrganizerTest::DeclaringAFolderThatIsNotOnDiskRefusesInsteadOfCreatingIt()
{
    Fixture f;
    const std::filesystem::path gone = std::filesystem::path(kLibrary) / "Sounds";

    const FileOperationResult result = f.organizer.DeclareCategory(f.profile, gone);

    QCOMPARE(result.result, FileResult::CouldNotCreateTheCategory);
    QVERIFY2(!f.fileSystem.IsDirectory(gone),
             "declaring a category the old program named built a folder the disk lost");
}

void LibraryOrganizerTest::EmptyingACategoryLeavesItDeclaredSoItStaysAMoveTarget()
{
    Fixture f;

    QCOMPARE(f.organizer.Move(f.profile, {AddonMove{kAddon, kAircrafts2024}}).front().result, FileResult::Completed);

    QVERIFY(f.fileSystem.Exists(CategoryMarkerPathIn(kAircrafts)));
    QVERIFY(!f.fileSystem.Exists(CategoryMarkerPathIn(kAircrafts2024)));
}

void LibraryOrganizerTest::AnAddonLeavingTheLibraryRootDoesNotDeclareTheRootACategory()
{
    Fixture f;
    const std::filesystem::path loose = kLibrary / "sim-rate-selector";
    f.fileSystem.AddDirectory(loose);
    f.fileSystem.AddFile(loose / "manifest.json", kMegabyte);
    f.TheLibraryHolds({kAddon, loose});

    QCOMPARE(f.organizer.Move(f.profile, {AddonMove{loose, kAircrafts}}).front().result, FileResult::Completed);

    QVERIFY(!f.fileSystem.Exists(CategoryMarkerPathIn(kLibrary)));
}

void LibraryOrganizerTest::ACategoryIsNotCreatedOutsideALibrary()
{
    const Fixture f;

    QCOMPARE(f.organizer.CreateCategory(f.profile, kDestination, "Sceneries").result,
             FileResult::TheTargetIsNotInALibrary);
    QVERIFY(!f.fileSystem.Exists("E:/Sim/Community/Sceneries"));
    QVERIFY(f.journal.appended.empty());
}

void LibraryOrganizerTest::AnEmptyCategoryIsRemovedAlongWithItsMarkerAndItsOverride()
{
    Fixture f;
    f.TheLibraryIsMadeOf({CategoryNode(kAircrafts, {AddonNode(kAddon)}), DeclaredCategoryNode(kAircrafts2024)});
    f.fileSystem.AddFile(CategoryMarkerPathIn(kAircrafts2024));
    f.profile.destinationOverrides.push_back(
        {.libraryId = "lib-1", .relativePath = "Aircrafts (2024)", .destination = kOtherDestination});

    const FileOperationResult result = f.organizer.RemoveCategory(f.profile, kAircrafts2024);

    QCOMPARE(result.result, FileResult::Completed);
    QVERIFY(!f.fileSystem.Exists(kAircrafts2024));
    QVERIFY(!f.fileSystem.Exists(CategoryMarkerPathIn(kAircrafts2024)));
    QVERIFY(f.profile.destinationOverrides.empty());
    QCOMPARE(f.journal.appended.front().kind, OperationKind::RemoveCategory);
}

void LibraryOrganizerTest::AStaleOverrideBelowTheRemovedCategoryIsForgottenToo()
{
    Fixture f;
    f.TheLibraryIsMadeOf({CategoryNode(kAircrafts, {AddonNode(kAddon)}), DeclaredCategoryNode(kAircrafts2024)});
    f.fileSystem.AddFile(CategoryMarkerPathIn(kAircrafts2024));
    f.profile.destinationOverrides.push_back(
        {.libraryId = "lib-1", .relativePath = "Aircrafts (2024)/gone-from-disk", .destination = kOtherDestination});
    f.profile.destinationOverrides.push_back(
        {.libraryId = "lib-1", .relativePath = "Aircrafts", .destination = kOtherDestination});

    const FileOperationResult result = f.organizer.RemoveCategory(f.profile, kAircrafts2024);

    QCOMPARE(result.result, FileResult::Completed);
    QCOMPARE(f.profile.destinationOverrides.size(), std::size_t{1});
    QCOMPARE(f.profile.destinationOverrides.front().relativePath, std::filesystem::path{"Aircrafts"});
}

void LibraryOrganizerTest::ACategoryThatStillHoldsAnAddonIsNotRemovedEvenWhenTheTreeSaysItIsEmpty()
{
    Fixture f;
    f.TheLibraryIsMadeOf({CategoryNode(kAircrafts, {AddonNode(kAddon)})});

    const FileOperationResult result = f.organizer.RemoveCategory(f.profile, kAircrafts);

    QCOMPARE(result.result, FileResult::TheCategoryStillHoldsAddons);
    QVERIFY(f.fileSystem.Exists(kAircrafts));
    QVERIFY(f.fileSystem.Exists(kAddon));
    QVERIFY(f.journal.appended.empty());
}

void LibraryOrganizerTest::AFolderTheScanNeverSawIsRefusedInsteadOfRemovedOnFaith()
{
    Fixture f;
    const std::filesystem::path unseen = kLibrary / "Sceneries";
    f.fileSystem.AddDirectory(unseen);
    f.TheLibraryIsMadeOf({CategoryNode(kAircrafts, {AddonNode(kAddon)})});

    QCOMPARE(f.organizer.RemoveCategory(f.profile, unseen).result, FileResult::TheTargetIsNotInALibrary);
    QVERIFY(f.fileSystem.Exists(unseen));
    QVERIFY(f.journal.appended.empty());
}

void LibraryOrganizerTest::TheLibraryRootIsNeverRemovedAsIfItWereACategory()
{
    Fixture f;

    QCOMPARE(f.organizer.RemoveCategory(f.profile, kLibrary).result, FileResult::TheTargetIsNotInALibrary);
    QVERIFY(f.fileSystem.Exists(kLibrary));
}

void LibraryOrganizerTest::NoCategoryIsRemovedWhileTheSimulatorIsRunning()
{
    Fixture f;
    f.TheLibraryIsMadeOf({DeclaredCategoryNode(kAircrafts2024)});
    f.processProbe.ReportTheSimulatorAsRunning();

    QCOMPARE(f.organizer.RemoveCategory(f.profile, kAircrafts2024).result, FileResult::TheSimulatorIsRunning);
    QVERIFY(f.fileSystem.Exists(kAircrafts2024));
}

void LibraryOrganizerTest::RenamingACategoryCarriesItsEnabledAddonsAlong()
{
    Fixture f;
    f.EnableTheAddonIn(kDestination);

    const FileOperationResult result = f.organizer.RenameCategory(f.profile, kAircrafts, "Airplanes");

    QCOMPARE(result.result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists("D:/Library/Airplanes/aerosoft-crj/manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kAircrafts));
    QVERIFY(f.fileSystem.IsLink(kLink));
    QCOMPARE(f.fileSystem.LinkTarget(kLink).value(), std::filesystem::path{"D:/Library/Airplanes/aerosoft-crj"});
}

void LibraryOrganizerTest::AnOverrideCarriedByARenameKeepsTheSpellingItWasWrittenWith()
{
    Fixture f;
    f.EnableTheAddonIn(kDestination);
    f.profile.destinationOverrides.push_back(
        {.libraryId = "lib-1", .relativePath = "Aircrafts/Aerosoft-CRJ", .destination = kOtherDestination});

    QCOMPARE(f.organizer.RenameCategory(f.profile, kAircrafts, "Airplanes").result, FileResult::Completed);

    QCOMPARE(f.profile.destinationOverrides.front().relativePath, std::filesystem::path{"Airplanes/Aerosoft-CRJ"});
}

void LibraryOrganizerTest::ABatchOfMovesClassifiesTheDestinationsOncePerLibrary()
{
    Fixture f;
    const std::filesystem::path other = "D:/Library/Aircrafts/fenix-a320";
    f.fileSystem.AddDirectory(other);
    f.fileSystem.AddFile(other / "manifest.json", kMegabyte);
    f.TheLibraryHolds({kAddon, other});
    f.EnableTheAddonIn(kDestination);

    f.filesystemProbe.enumerated.clear();

    const std::vector<FileOperationResult> results =
        f.organizer.Move(f.profile,
                         {AddonMove{.addonFolder = kAddon, .category = kAircrafts2024},
                          AddonMove{.addonFolder = other, .category = kAircrafts2024}});

    QCOMPARE(results.size(), std::size_t{2});
    QCOMPARE(results.front().result, FileResult::Completed);
    QCOMPARE(results.back().result, FileResult::Completed);
    QVERIFY2(f.filesystemProbe.TimesEnumerated(kDestination) == std::size_t{1},
             "the batch pays one destination classification per library, not one per move");
}

QTEST_APPLESS_MAIN(LibraryOrganizerTest)

#include "tst_library_organizer.moc"
