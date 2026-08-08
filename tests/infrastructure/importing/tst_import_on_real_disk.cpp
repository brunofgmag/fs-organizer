#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <variant>

#include <fstream>
#include <system_error>

#include "application/ImportService.h"
#include "application/LibraryOrganizer.h"
#include "domain/importing/ImportEngine.h"
#include "domain/importing/ImportPaths.h"
#include "domain/importing/OriginSidecar.h"
#include "domain/journal/OperationLog.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "tests/support/DeepPaths.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class ImportOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void APhysicalAddonReallyMovesIntoTheLibraryAndLeavesAJunctionBehind();
        static void AnImportIntoAFolderThatDoesNotExistYetStillKnowsTheFreeSpace();
        static void TheSourceSurvivesWhenTheCopyFails();
        static void ALiveJunctionOfAnotherProgramIsNeverReplaced();
        static void TheFirstQuarantineOfALibraryCreatesTheFolderItNeeds();
        static void RestoringPutsTheAddonBackEvenWithoutItsCategoryFolder();
        static void MovingAnEnabledAddonReallyCarriesItsJunctionToTheNewFolder();
        static void ACreatedCategoryIsARealFolderAndTheSecondAttemptIsRefused();
        static void AStagingLeftoverPastTheOldCeilingIsFoundAndTheDiscardEmptiesTheQueue();
        static void AStagingLeftoverPastTheOldCeilingResumesIntoAFinishedImport();
        static void TheQuarantineDetailReadsTheVersionOutOfARealManifestOnBothSides();
        static void TheRestoreGuardTellsATakenIdentityFromAnOccupiedOriginOnRealDisk();
        static void AnItemWithNoOriginIsOfferedTheRealCategoriesOfTheLibraryHoldingIt();
        static void RestoringIntoTheChosenCategoryReallyMovesTheFolderThere();
        static void TheOriginBesideTheItemOutlivesTheJournalFile();
        static void TheRecordBesideTheItemIsNeverEnumeratedAsAnItem();
        static void AnAccentedOriginComesBackByteForByteFromTheRealFile();
        static void TheSwapReallyExchangesTheOccupantAndTheItemOnDisk();
    };
}

namespace
{
    struct Disk
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return directory.path().toStdWString();
        }

        [[nodiscard]] std::filesystem::path Destination() const
        {
            return Root() / "Sim" / "Community";
        }

        [[nodiscard]] std::filesystem::path Category() const
        {
            return Root() / "Library" / "Utils";
        }

        [[nodiscard]] std::filesystem::path AddFolder(const std::filesystem::path& relative) const
        {
            const std::filesystem::path folder = Root() / relative;
            std::filesystem::create_directories(folder);

            return folder;
        }

        void AddFile(const std::filesystem::path& relative, const std::string& content) const
        {
            const std::filesystem::path file = Root() / relative;
            std::filesystem::create_directories(file.parent_path());

            std::ofstream stream(file, std::ios::binary);
            stream << content;
        }

        [[nodiscard]] SimulatorProfile Profile() const
        {
            SimulatorProfile profile;
            profile.destinations = {Destination()};
            profile.defaultDestination = Destination();
            profile.libraries = {Library{.id = "lib-1", .path = Root() / "Library", .label = "Biblioteca"}};

            return profile;
        }
    };

    struct Engine
    {
        WindowsLinkService linkService{};
        WindowsFilesystemProbe filesystemProbe{};
        WindowsFileOperations files{};
        LinkingEngine linking{linkService, filesystemProbe};
        SystemClock clock{};
        std::filesystem::path journalFile{};
        JsonlOperationJournal journal{journalFile};
        OperationLog log{journal, clock};
        ImportEngine engine{filesystemProbe, files, linking, log, LinkType::Junction};
    };

    struct Service
    {
        Engine engine{};
        WindowsProcessProbe processProbe{std::vector<std::string>{}};
        JsonManifestParser manifestParser{};
        FilesystemScanner catalog{manifestParser, engine.filesystemProbe};
        ImportService service{engine.engine,  processProbe, engine.filesystemProbe, catalog, engine.files,
                              engine.linking, engine.log,   LinkType::Junction};
        EntryClassifier classifier{engine.linkService, engine.filesystemProbe};
        LibraryOrganizer organizer{catalog,    engine.filesystemProbe, engine.files, engine.linking,
                                   classifier, processProbe,           engine.log,   LinkType::Junction};

        [[nodiscard]] std::vector<DestinationEntry> Entries(const Disk& disk) const
        {
            return classifier.Resolve(disk.Profile().destinations, {disk.Root() / "Library"});
        }
    };

    [[nodiscard]] SimulatorProfile ProfileWithTheLibraryAt(const Disk& disk, const std::filesystem::path& library)
    {
        SimulatorProfile profile;
        profile.destinations = {disk.Destination()};
        profile.defaultDestination = disk.Destination();
        profile.libraries = {Library{.id = "lib-1", .path = library, .label = "Biblioteca"}};

        return profile;
    }

    std::string ContentsOfFile(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);

        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    void PutTwoCopiesOfTheSameAddonOnDisk(const Disk& disk, const std::string& name)
    {
        disk.AddFile("Library/Utils/" + name + "/manifest.json", R"({"title": "MD-11", "package_version": "0.6.3"})");
        disk.AddFile("Library/Utils/" + name + "/aircraft.cfg", "the library copy");
        disk.AddFile("Sim/Community/" + name + "/manifest.json", R"({"title": "MD-11", "package_version": "0.7.0"})");
        disk.AddFile("Sim/Community/" + name + "/aircraft.cfg", "the destination copy");
    }
}

void ImportOnRealDiskTest::APhysicalAddonReallyMovesIntoTheLibraryAndLeavesAJunctionBehind()
{
    const Disk disk;
    static_cast<void>(disk.AddFolder("Library/Utils"));
    disk.AddFile("Sim/Community/simbridge/manifest.json", R"({"title": "SimBridge"})");
    disk.AddFile("Sim/Community/simbridge/dist/simbridge.exe", std::string(4096, 'x'));

    const Engine engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"};

    const ImportRequest request{.source = disk.Destination() / "simbridge", .category = disk.Category()};
    const ImportOutcome outcome = engine.engine.Import(disk.Profile(), request, {});

    QCOMPARE(outcome.Result(), FileResult::Completed);

    const std::filesystem::path landed = disk.Category() / "simbridge";
    QVERIFY(std::filesystem::exists(landed / "manifest.json"));
    QCOMPARE(std::filesystem::file_size(landed / "dist" / "simbridge.exe"), std::uintmax_t{4096});
    QVERIFY(!std::filesystem::exists(disk.Category() / "simbridge.fsorg-partial"));

    QVERIFY(engine.filesystemProbe.IsReparsePoint(request.source));
    QCOMPARE(NormalizeReparseTarget(engine.linkService.ReadLinkTarget(request.source).value()), landed);
    QVERIFY(std::filesystem::exists(request.source / "manifest.json"));

    QCOMPARE(engine.journal.Read().size(), std::size_t{5});
    QCOMPARE(engine.journal.Read().back().kind, OperationKind::EnableAddon);
    QCOMPARE(std::get<FileResult>(engine.journal.Read().front().outcome), FileResult::Completed);
}

void ImportOnRealDiskTest::AnImportIntoAFolderThatDoesNotExistYetStillKnowsTheFreeSpace()
{
    const Disk disk;
    static_cast<void>(disk.AddFolder("Library/Utils"));
    disk.AddFile("Sim/Community/tlc-bgjn/manifest.json", R"({"title": "Ilulissat"})");

    const WindowsFilesystemProbe filesystemProbe;

    QVERIFY(filesystemProbe.FreeSpaceOn(disk.Category()).has_value());
    QVERIFY(!filesystemProbe.FreeSpaceOn(disk.Category() / "tlc-bgjn").has_value());

    const Engine engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"};

    QCOMPARE(engine.engine
                 .Import(disk.Profile(),
                         ImportRequest{.source = disk.Destination() / "tlc-bgjn", .category = disk.Category()}, {})
                 .Result(),
             FileResult::Completed);
}

void ImportOnRealDiskTest::TheSourceSurvivesWhenTheCopyFails()
{
    const Disk disk;
    static_cast<void>(disk.AddFolder("Library/Utils"));
    disk.AddFile("Sim/Community/fenix-a320/manifest.json", R"({"title": "A320"})");
    disk.AddFile("Library/Utils/fenix-a320.fsorg-partial/manifest.json", "restos de outra tentativa");

    const Engine engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"};

    const ImportRequest request{.source = disk.Destination() / "fenix-a320", .category = disk.Category()};
    const ImportOutcome outcome = engine.engine.Import(disk.Profile(), request, {});

    QCOMPARE(outcome.Result(), FileResult::CouldNotCopy);
    QVERIFY(std::filesystem::exists(request.source / "manifest.json"));
    QVERIFY(!std::filesystem::exists(disk.Category() / "fenix-a320"));
    QCOMPARE(engine.journal.Read().size(), std::size_t{1});
    QCOMPARE(std::get<FileResult>(engine.journal.Read().front().outcome), FileResult::CouldNotCopy);
}

void ImportOnRealDiskTest::ALiveJunctionOfAnotherProgramIsNeverReplaced()
{
    const Disk disk;
    const std::filesystem::path foreign = disk.AddFolder("Outro Programa/fsdreamteam-gsx-pro");
    disk.AddFile("Outro Programa/fsdreamteam-gsx-pro/manifest.json", R"({"title": "GSX"})");
    const std::filesystem::path library = disk.AddFolder("Library/Utils/fsdreamteam-gsx-pro");
    static_cast<void>(disk.AddFolder("Sim/Community"));

    Engine engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"};

    const std::filesystem::path linkPath = disk.Destination() / "fsdreamteam-gsx-pro";
    QCOMPARE(engine.linkService.CreateLink(linkPath, foreign, LinkType::Junction), LinkFailure::None);

    const LinkOutcome outcome =
        engine.linking.Enable(Addon{.folderPath = library}, disk.Destination(), LinkType::Junction);

    QCOMPARE(outcome.Failure(), LinkFailure::DestinationHoldsLiveLink);
    QVERIFY(outcome.Occupation() != nullptr);
    QCOMPARE(outcome.Occupation()->existingTarget, foreign);
    QCOMPARE(NormalizeReparseTarget(engine.linkService.ReadLinkTarget(linkPath).value()), foreign);
    QVERIFY(std::filesystem::exists(foreign / "manifest.json"));
}

void ImportOnRealDiskTest::TheFirstQuarantineOfALibraryCreatesTheFolderItNeeds()
{
    const Disk disk;
    PutTwoCopiesOfTheSameAddonOnDisk(disk, "tfdidesign-aircraft-md11");

    const Service composed{.engine = {.journalFile = disk.Root() / "journal" / "operations.jsonl"}};

    const std::filesystem::path quarantine = QuarantineFolderInside(disk.Root() / "Library");
    QVERIFY(!std::filesystem::exists(quarantine));

    const CopyConflict conflict{.provenancePath = disk.Destination() / "tfdidesign-aircraft-md11",
                                .libraryPath = disk.Category() / "tfdidesign-aircraft-md11"};
    const FileResult result = composed.service.ResolveConflict(disk.Profile(), composed.Entries(disk), conflict,
                                                               ConflictChoice::KeepTheProvenanceCopy);

    QCOMPARE(result, FileResult::Completed);
    QVERIFY(std::filesystem::exists(quarantine / "tfdidesign-aircraft-md11" / "manifest.json"));
    QVERIFY(!std::filesystem::exists(conflict.libraryPath));
    QVERIFY(std::filesystem::exists(conflict.provenancePath / "aircraft.cfg"));

    const std::vector<OperationRecord> history = composed.engine.journal.Read();
    QCOMPARE(history.size(), std::size_t{1});
    QCOMPARE(history.front().kind, OperationKind::QuarantineFromLibrary);
    QCOMPARE(std::get<FileResult>(history.front().outcome), FileResult::Completed);
}

void ImportOnRealDiskTest::RestoringPutsTheAddonBackEvenWithoutItsCategoryFolder()
{
    const Disk disk;
    PutTwoCopiesOfTheSameAddonOnDisk(disk, "tfdidesign-aircraft-md11");

    const Service composed{.engine = {.journalFile = disk.Root() / "journal" / "operations.jsonl"}};

    const CopyConflict conflict{.provenancePath = disk.Destination() / "tfdidesign-aircraft-md11",
                                .libraryPath = disk.Category() / "tfdidesign-aircraft-md11"};
    QCOMPARE(composed.service.ResolveConflict(disk.Profile(), composed.Entries(disk), conflict,
                                              ConflictChoice::KeepTheProvenanceCopy),
             FileResult::Completed);

    std::filesystem::remove_all(disk.Category());

    const std::vector<QuarantinedItem> quarantined = composed.service.Quarantined(disk.Profile());
    QCOMPARE(quarantined.size(), std::size_t{1});
    QCOMPARE(quarantined.front().origin, conflict.libraryPath);

    const std::vector<FileOperationResult> results = composed.service.Restore(disk.Profile(), quarantined);

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY(std::filesystem::exists(conflict.libraryPath / "aircraft.cfg"));
    QVERIFY(!std::filesystem::exists(QuarantineFolderInside(disk.Root() / "Library") / "tfdidesign-aircraft-md11"));
}

void ImportOnRealDiskTest::MovingAnEnabledAddonReallyCarriesItsJunctionToTheNewFolder()
{
    const Disk disk;
    static_cast<void>(disk.AddFolder("Sim/Community"));
    static_cast<void>(disk.AddFolder("Library/Aircrafts (2024)"));
    disk.AddFile("Library/Aircrafts/aerosoft-crj/manifest.json", R"({"title": "CRJ", "content_type": "AIRCRAFT"})");
    disk.AddFile("Library/Aircrafts/aerosoft-crj/SimObjects/plane.cfg", std::string(2048, 'c'));

    const Service service{.engine = Engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"}};
    SimulatorProfile profile = disk.Profile();

    const std::filesystem::path addon = disk.Root() / "Library" / "Aircrafts" / "aerosoft-crj";
    const std::filesystem::path link = disk.Destination() / "aerosoft-crj";

    QVERIFY(service.engine.linking
                .Enable(Addon{.folderPath = addon, .manifest = Manifest{}}, disk.Destination(), LinkType::Junction)
                .Succeeded());
    QVERIFY(service.engine.filesystemProbe.IsReparsePoint(link));

    const std::filesystem::path category = disk.Root() / "Library" / "Aircrafts (2024)";
    const std::vector<FileOperationResult> results =
        service.organizer.Move(profile, {AddonMove{.addonFolder = addon, .category = category}});

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::Completed);

    const std::filesystem::path landed = category / "aerosoft-crj";
    QVERIFY(std::filesystem::exists(landed / "manifest.json"));
    QCOMPARE(std::filesystem::file_size(landed / "SimObjects" / "plane.cfg"), std::uintmax_t{2048});
    QVERIFY(!std::filesystem::exists(addon));

    QVERIFY(service.engine.filesystemProbe.IsReparsePoint(link));
    QCOMPARE(NormalizeReparseTarget(service.engine.linkService.ReadLinkTarget(link).value()), landed);

    QCOMPARE(service.engine.journal.Read().size(), std::size_t{3});
    QCOMPARE(service.engine.journal.Read()[0].kind, OperationKind::DisableAddon);
    QCOMPARE(service.engine.journal.Read()[1].kind, OperationKind::MoveAddon);
    QCOMPARE(service.engine.journal.Read()[1].target, landed);
    QCOMPARE(service.engine.journal.Read()[2].kind, OperationKind::EnableAddon);
}

void ImportOnRealDiskTest::ACreatedCategoryIsARealFolderAndTheSecondAttemptIsRefused()
{
    const Disk disk;
    static_cast<void>(disk.AddFolder("Sim/Community"));
    static_cast<void>(disk.AddFolder("Library"));

    const Service service{.engine = Engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"}};
    const SimulatorProfile profile = disk.Profile();
    const std::filesystem::path library = disk.Root() / "Library";

    QCOMPARE(service.organizer.CreateCategory(profile, library, "Sceneries").result, FileResult::Completed);
    QVERIFY(std::filesystem::is_directory(library / "Sceneries"));

    QCOMPARE(service.organizer.CreateCategory(profile, library, "Sceneries").result,
             FileResult::CouldNotCreateTheCategory);

    QCOMPARE(service.organizer.CreateCategory(profile, disk.Destination(), "Sceneries").result,
             FileResult::TheTargetIsNotInALibrary);
    QVERIFY(!std::filesystem::exists(disk.Destination() / "Sceneries"));
}

void ImportOnRealDiskTest::AStagingLeftoverPastTheOldCeilingIsFoundAndTheDiscardEmptiesTheQueue()
{
    const Disk disk;
    const std::filesystem::path library = FolderPastTheCeiling(disk.Root(), "Library");
    const std::filesystem::path staging = library / "Utils" / "tfdidesign-aircraft-md11.fsorg-partial";
    QVERIFY(staging.wstring().size() > kOldPathCeiling);

    WriteFilePastTheCeiling(staging / "manifest.json", R"({"title": "MD-11"})");
    WriteFilePastTheCeiling(staging / "SimObjects" / "Airplanes" / "md11" / "model.gltf", "vertices");

    const Service composed{.engine = {.journalFile = disk.Root() / "journal" / "operations.jsonl"}};
    const SimulatorProfile profile = ProfileWithTheLibraryAt(disk, library);

    const std::vector<StagingLeftover> leftovers = composed.service.Leftovers(profile);
    QCOMPARE(leftovers.size(), std::size_t{1});
    QCOMPARE(leftovers.front().staging, staging);

    const std::vector<FileOperationResult> discarded = composed.service.DiscardLeftovers(profile, leftovers);
    QCOMPARE(discarded.size(), std::size_t{1});
    QCOMPARE(discarded.front().result, FileResult::Completed);

    QVERIFY(!ExistsPastTheCeiling(staging));
    QVERIFY(composed.service.Leftovers(profile).empty());
}

void ImportOnRealDiskTest::AStagingLeftoverPastTheOldCeilingResumesIntoAFinishedImport()
{
    const Disk disk;
    const std::filesystem::path library = FolderPastTheCeiling(disk.Root(), "Library");
    const std::filesystem::path category = FolderPastTheCeiling(library, "Utils");
    disk.AddFile("Sim/Community/tfdidesign-aircraft-md11/manifest.json", R"({"title": "MD-11"})");
    disk.AddFile("Sim/Community/tfdidesign-aircraft-md11/aircraft.cfg", "the destination copy");

    const std::filesystem::path target = category / "tfdidesign-aircraft-md11";
    const std::filesystem::path blocker = FolderPastTheCeiling(category, "tfdidesign-aircraft-md11");
    QCOMPARE(blocker, target);

    const Service composed{.engine = {.journalFile = disk.Root() / "journal" / "operations.jsonl"}};
    const SimulatorProfile profile = ProfileWithTheLibraryAt(disk, library);
    const ImportRequest request{.source = disk.Destination() / "tfdidesign-aircraft-md11", .category = category};

    QCOMPARE(composed.engine.engine.Import(profile, request, {}).Result(), FileResult::CouldNotMoveIntoPlace);

    const std::vector<StagingLeftover> leftovers = composed.service.Leftovers(profile);
    QCOMPARE(leftovers.size(), std::size_t{1});
    QCOMPARE(leftovers.front().staging, StagingPathFor(target));
    QVERIFY(leftovers.front().CanBeResumed());
    QCOMPARE(leftovers.front().source, request.source);

    QVERIFY(std::filesystem::remove(BeyondTheCeiling(target)));

    const std::vector<ImportOperationResult> resumed = composed.service.Resume(profile, leftovers, {});
    QCOMPARE(resumed.size(), std::size_t{1});
    QCOMPARE(resumed.front().result, FileResult::Completed);

    QVERIFY(ExistsPastTheCeiling(target / "aircraft.cfg"));
    QVERIFY(!ExistsPastTheCeiling(StagingPathFor(target)));
    QVERIFY(composed.service.Leftovers(profile).empty());
    QCOMPARE(NormalizeReparseTarget(composed.engine.linkService.ReadLinkTarget(request.source).value()), target);
}

void ImportOnRealDiskTest::TheQuarantineDetailReadsTheVersionOutOfARealManifestOnBothSides()
{
    const Disk disk;
    const Service composed;

    disk.AddFile("Library/_fsorganizer-quarantine/simbridge/manifest.json",
                 R"({"title": "SimBridge", "package_version": "0.6.3"})");
    disk.AddFile("Sim/Community/simbridge/manifest.json", R"({"title": "SimBridge", "package_version": "0.7.0"})");
    disk.AddFile("Sim/Community/nothing-declared/marker.txt", "no manifest here");

    const std::vector<QuarantinedItem> held = composed.service.Quarantined(disk.Profile());
    QCOMPARE(held.size(), std::size_t{1});

    const std::vector<QuarantineDetail> details = composed.service.Describe(composed.Entries(disk), held);

    QCOMPARE(details.size(), std::size_t{1});
    QCOMPARE(details.front().version, std::string{"0.6.3"});
    QVERIFY(details.front().WasReplaced());
    QCOMPARE(details.front().replacedBy, disk.Destination() / "simbridge");
    QCOMPARE(details.front().replacementVersion, std::string{"0.7.0"});
}

void ImportOnRealDiskTest::TheRestoreGuardTellsATakenIdentityFromAnOccupiedOriginOnRealDisk()
{
    const Disk disk;
    const Service composed;

    disk.AddFile("Library/_fsorganizer-quarantine/simbridge/manifest.json",
                 R"({"title": "SimBridge", "package_version": "0.6.3"})");
    disk.AddFile("Library/Utils/simbridge/manifest.json", R"({"title": "SimBridge", "package_version": "0.7.0"})");

    const std::filesystem::path held = disk.Root() / "Library" / "_fsorganizer-quarantine" / "simbridge";
    const std::vector<QuarantinedItem> items{QuarantinedItem{.path = held, .origin = disk.Category() / "simbridge"}};

    const std::vector<RestoreCheck> occupied = composed.service.CheckRestore(disk.Profile(), items);

    QCOMPARE(occupied.size(), std::size_t{1});
    QCOMPARE(occupied.front().result, FileResult::TheIdentityIsTaken);
    QCOMPARE(occupied.front().occupant, disk.Category() / "simbridge");
    QCOMPARE(occupied.front().version, std::string{"0.6.3"});
    QCOMPARE(occupied.front().occupantVersion, std::string{"0.7.0"});

    static_cast<void>(disk.AddFolder("Library/Sceneries/simbridge"));
    const std::vector<QuarantinedItem> intoSceneries{
        QuarantinedItem{.path = held, .origin = disk.Root() / "Library" / "Sceneries" / "simbridge"}};

    const std::vector<RestoreCheck> taken = composed.service.CheckRestore(disk.Profile(), intoSceneries);

    QCOMPARE(taken.front().result, FileResult::TheIdentityIsTaken);
    QCOMPARE(taken.front().occupant, disk.Category() / "simbridge");

    QVERIFY(std::filesystem::remove_all(disk.Category() / "simbridge") > 0);

    const std::vector<RestoreCheck> onlyTheFolder = composed.service.CheckRestore(disk.Profile(), intoSceneries);

    QCOMPARE(onlyTheFolder.front().result, FileResult::TheOriginIsOccupied);
    QCOMPARE(onlyTheFolder.front().occupant, disk.Root() / "Library" / "Sceneries" / "simbridge");
    QVERIFY(onlyTheFolder.front().occupantVersion.empty());
    QCOMPARE(onlyTheFolder.front().version, std::string{"0.6.3"});
}

void ImportOnRealDiskTest::AnItemWithNoOriginIsOfferedTheRealCategoriesOfTheLibraryHoldingIt()
{
    const Disk disk;
    const Service composed;

    disk.AddFile("Library/_fsorganizer-quarantine/simbridge/manifest.json", R"({"title": "SimBridge"})");
    disk.AddFile("Library/Utils/pmdg-aircraft-77w/manifest.json", R"({"title": "77W"})");
    static_cast<void>(disk.AddFolder("Sim/Community"));

    const std::filesystem::path held = disk.Root() / "Library" / "_fsorganizer-quarantine" / "simbridge";

    const std::vector<QuarantinedItem> items{QuarantinedItem{.path = held}};
    QCOMPARE(composed.service.CheckRestore(disk.Profile(), items).front().result, FileResult::TheOriginIsUnknown);

    const std::vector<RestorePlace> places = composed.service.PlacesFor(disk.Profile(), items.front());

    QCOMPARE(places.size(), std::size_t{2});
    QCOMPARE(places.front().place, disk.Root() / "Library");
    QCOMPARE(places.back().place, disk.Category());
    QCOMPARE(places.back().target, disk.Category() / "simbridge");
    QCOMPARE(places.back().label, std::filesystem::path{"Utils"});
}

void ImportOnRealDiskTest::RestoringIntoTheChosenCategoryReallyMovesTheFolderThere()
{
    const Disk disk;
    const Service composed;

    disk.AddFile("Library/_fsorganizer-quarantine/simbridge/manifest.json", R"({"title": "SimBridge"})");
    disk.AddFile("Library/_fsorganizer-quarantine/simbridge/dist/simbridge.exe", std::string(2048, 'x'));
    disk.AddFile("Library/Utils/pmdg-aircraft-77w/manifest.json", R"({"title": "77W"})");
    static_cast<void>(disk.AddFolder("Sim/Community"));

    const std::filesystem::path held = disk.Root() / "Library" / "_fsorganizer-quarantine" / "simbridge";
    const std::vector<RestorePlace> places = composed.service.PlacesFor(disk.Profile(), QuarantinedItem{.path = held});

    const auto chosen = std::ranges::find_if(places,
                                             [&disk](const RestorePlace& place)
                                             {
                                                 return place.place == disk.Category();
                                             });
    QVERIFY(chosen != places.end());

    const std::vector<FileOperationResult> restored =
        composed.service.Restore(disk.Profile(), {QuarantinedItem{.path = held, .origin = chosen->target}});

    QCOMPARE(restored.size(), std::size_t{1});
    QCOMPARE(restored.front().result, FileResult::Completed);
    QVERIFY(std::filesystem::exists(disk.Category() / "simbridge" / "dist" / "simbridge.exe"));
    QVERIFY(!std::filesystem::exists(held));
}

void ImportOnRealDiskTest::TheOriginBesideTheItemOutlivesTheJournalFile()
{
    const Disk disk;
    PutTwoCopiesOfTheSameAddonOnDisk(disk, "tfdidesign-aircraft-md11");

    const std::filesystem::path journalFile = disk.Root() / "journal" / "operations.jsonl";
    const std::filesystem::path held = QuarantineFolderInside(disk.Root() / "Library") / "tfdidesign-aircraft-md11";

    const CopyConflict conflict{.provenancePath = disk.Destination() / "tfdidesign-aircraft-md11",
                                .libraryPath = disk.Category() / "tfdidesign-aircraft-md11"};

    {
        const Service wrote{.engine = {.journalFile = journalFile}};

        QCOMPARE(wrote.service.ResolveConflict(disk.Profile(), wrote.Entries(disk), conflict,
                                               ConflictChoice::KeepTheProvenanceCopy),
                 FileResult::Completed);
    }

    QVERIFY(std::filesystem::exists(SidecarPathFor(held)));

    std::error_code error;
    QVERIFY(std::filesystem::remove(journalFile, error));

    const Service afterwards{.engine = {.journalFile = journalFile}};

    QVERIFY(afterwards.engine.journal.Read().empty());

    const std::vector<QuarantinedItem> items = afterwards.service.Quarantined(disk.Profile());

    QCOMPARE(items.size(), std::size_t{1});
    QCOMPARE(items.front().origin, conflict.libraryPath);
    QCOMPARE(items.front().source, OriginSource::Sidecar);
    QVERIFY(items.front().quarantinedAt.has_value());

    const std::vector<FileOperationResult> restored = afterwards.service.Restore(disk.Profile(), items);

    QCOMPARE(restored.front().result, FileResult::Completed);
    QVERIFY(std::filesystem::exists(conflict.libraryPath / "aircraft.cfg"));
    QVERIFY(!std::filesystem::exists(SidecarPathFor(held)));
}

void ImportOnRealDiskTest::TheRecordBesideTheItemIsNeverEnumeratedAsAnItem()
{
    const Disk disk;
    PutTwoCopiesOfTheSameAddonOnDisk(disk, "tfdidesign-aircraft-md11");

    const Service composed{.engine = {.journalFile = disk.Root() / "journal" / "operations.jsonl"}};

    const CopyConflict conflict{.provenancePath = disk.Destination() / "tfdidesign-aircraft-md11",
                                .libraryPath = disk.Category() / "tfdidesign-aircraft-md11"};

    QCOMPARE(composed.service.ResolveConflict(disk.Profile(), composed.Entries(disk), conflict,
                                              ConflictChoice::KeepTheProvenanceCopy),
             FileResult::Completed);

    const std::filesystem::path quarantine = QuarantineFolderInside(disk.Root() / "Library");

    QCOMPARE(std::distance(std::filesystem::directory_iterator(quarantine), std::filesystem::directory_iterator{}),
             std::ptrdiff_t{2});
    QCOMPARE(composed.engine.filesystemProbe.ChildDirectories(quarantine).size(), std::size_t{1});
    QCOMPARE(composed.service.Quarantined(disk.Profile()).size(), std::size_t{1});
}

void ImportOnRealDiskTest::AnAccentedOriginComesBackByteForByteFromTheRealFile()
{
    const Disk disk;
    Service composed;

    const std::filesystem::path quarantine = QuarantineFolderInside(disk.Root() / "Library");
    const std::filesystem::path held = quarantine / "aviao";

    static_cast<void>(disk.AddFolder("Library/_fsorganizer-quarantine/aviao"));

    const std::filesystem::path accented = disk.Category()
        / PathFromUtf8("Avi\xC3\xB5"
                       "es");

    QVERIFY(composed.engine.files.WriteTextFile(SidecarPathFor(held),
                                                TextOfTheOrigin(QuarantineOrigin{.origin = accented})));

    const std::vector<QuarantinedItem> items = composed.service.Quarantined(disk.Profile());

    QCOMPARE(items.size(), std::size_t{1});
    QCOMPARE(items.front().origin, accented);
    QCOMPARE(items.front().source, OriginSource::Sidecar);
}

void ImportOnRealDiskTest::TheSwapReallyExchangesTheOccupantAndTheItemOnDisk()
{
    const Disk disk;
    Service composed{.engine = {.journalFile = disk.Root() / "journal" / "operations.jsonl"}};

    disk.AddFile("Library/_fsorganizer-quarantine/simbridge/manifest.json",
                 R"({"title": "SimBridge", "package_version": "0.6.3"})");
    disk.AddFile("Library/_fsorganizer-quarantine/simbridge/dist/simbridge.exe", "the older build");
    disk.AddFile("Library/Utils/simbridge/manifest.json", R"({"title": "SimBridge", "package_version": "0.7.0"})");
    disk.AddFile("Library/Utils/simbridge/dist/simbridge.exe", "the newer build");

    const std::filesystem::path held = QuarantineFolderInside(disk.Root() / "Library") / "simbridge";
    const std::filesystem::path place = disk.Category() / "simbridge";

    QVERIFY(
        composed.engine.files.WriteTextFile(SidecarPathFor(held), TextOfTheOrigin(QuarantineOrigin{.origin = place})));

    const QuarantinedItem item = composed.service.Quarantined(disk.Profile()).front();
    const SwapResult swapped = composed.service.Swap(disk.Profile(), composed.Entries(disk), item);

    QVERIFY(swapped.Succeeded());
    QCOMPARE(swapped.stoppedAt, SwapStep::RestoreTheItem);
    QCOMPARE(swapped.inTheLibrary, place);

    QCOMPARE(ContentsOfFile(place / "dist" / "simbridge.exe"), std::string{"the older build"});
    QCOMPARE(ContentsOfFile(held / "dist" / "simbridge.exe"), std::string{"the newer build"});
    QVERIFY(!std::filesystem::exists(SwapSlotFor(held)));

    const std::optional<QuarantineOrigin> recorded =
        OriginFromText(composed.engine.filesystemProbe.ContentsOf(SidecarPathFor(held)).value_or(std::string{}));

    QVERIFY(recorded.has_value());
    QCOMPARE(recorded->origin, place);
}

QTEST_APPLESS_MAIN(ImportOnRealDiskTest)

#include "tst_import_on_real_disk.moc"
