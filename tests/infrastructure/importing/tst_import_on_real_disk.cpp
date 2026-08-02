#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <variant>

#include <fstream>

#include "application/ImportService.h"
#include "application/LibraryOrganizer.h"
#include "domain/importing/ImportEngine.h"
#include "domain/importing/ImportPaths.h"
#include "domain/journal/OperationLog.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
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
    };
}

namespace
{
    struct Disk
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return std::filesystem::path(directory.path().toStdWString());
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
            profile.libraries = {Library{.id = "lib-1", .path = Root() / "Library", .label = "Library"}};

            return profile;
        }
    };

    struct Engine
    {
        WindowsLinkService linkService;
        WindowsFilesystemProbe filesystemProbe;
        WindowsFileOperations files;
        LinkingEngine linking{linkService, filesystemProbe};
        SystemClock clock;
        std::filesystem::path journalFile;
        JsonlOperationJournal journal{journalFile};
        OperationLog log{journal, clock};
        ImportEngine engine{filesystemProbe, files, linking, log, LinkType::Junction};
    };

    struct Service
    {
        Engine engine;
        WindowsProcessProbe processProbe{std::vector<std::string>{}};
        JsonManifestParser manifestParser;
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

    Engine engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"};

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

    Engine engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"};

    QCOMPARE(engine.engine.Import(disk.Profile(), ImportRequest{disk.Destination() / "tlc-bgjn", disk.Category()}, {})
                 .Result(),
             FileResult::Completed);
}

void ImportOnRealDiskTest::TheSourceSurvivesWhenTheCopyFails()
{
    const Disk disk;
    static_cast<void>(disk.AddFolder("Library/Utils"));
    disk.AddFile("Sim/Community/fenix-a320/manifest.json", R"({"title": "A320"})");
    disk.AddFile("Library/Utils/fenix-a320.fsorg-partial/manifest.json", "restos de outra tentativa");

    Engine engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"};

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
    QVERIFY(outcome.Occupation().has_value());
    QCOMPARE(outcome.Occupation()->existingTarget, foreign);
    QCOMPARE(NormalizeReparseTarget(engine.linkService.ReadLinkTarget(linkPath).value()), foreign);
    QVERIFY(std::filesystem::exists(foreign / "manifest.json"));
}

void ImportOnRealDiskTest::TheFirstQuarantineOfALibraryCreatesTheFolderItNeeds()
{
    const Disk disk;
    PutTwoCopiesOfTheSameAddonOnDisk(disk, "tfdidesign-aircraft-md11");

    Service composed{.engine = {.journalFile = disk.Root() / "journal" / "operations.jsonl"}};

    const std::filesystem::path quarantine = QuarantineFolderInside(disk.Root() / "Library");
    QVERIFY(!std::filesystem::exists(quarantine));

    const CopyConflict conflict{.destinationPath = disk.Destination() / "tfdidesign-aircraft-md11",
                                .libraryPath = disk.Category() / "tfdidesign-aircraft-md11"};
    const FileResult result = composed.service.ResolveConflict(disk.Profile(), composed.Entries(disk), conflict,
                                                               ConflictChoice::KeepTheDestinationCopy);

    QCOMPARE(result, FileResult::Completed);
    QVERIFY(std::filesystem::exists(quarantine / "tfdidesign-aircraft-md11" / "manifest.json"));
    QVERIFY(!std::filesystem::exists(conflict.libraryPath));
    QVERIFY(std::filesystem::exists(conflict.destinationPath / "aircraft.cfg"));

    const std::vector<OperationRecord> history = composed.engine.journal.Read();
    QCOMPARE(history.size(), std::size_t{1});
    QCOMPARE(history.front().kind, OperationKind::QuarantineFromLibrary);
    QCOMPARE(std::get<FileResult>(history.front().outcome), FileResult::Completed);
}

void ImportOnRealDiskTest::RestoringPutsTheAddonBackEvenWithoutItsCategoryFolder()
{
    const Disk disk;
    PutTwoCopiesOfTheSameAddonOnDisk(disk, "tfdidesign-aircraft-md11");

    Service composed{.engine = {.journalFile = disk.Root() / "journal" / "operations.jsonl"}};

    const CopyConflict conflict{.destinationPath = disk.Destination() / "tfdidesign-aircraft-md11",
                                .libraryPath = disk.Category() / "tfdidesign-aircraft-md11"};
    QCOMPARE(composed.service.ResolveConflict(disk.Profile(), composed.Entries(disk), conflict,
                                              ConflictChoice::KeepTheDestinationCopy),
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

    Service service{.engine = Engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"}};
    SimulatorProfile profile = disk.Profile();

    const std::filesystem::path addon = disk.Root() / "Library" / "Aircrafts" / "aerosoft-crj";
    const std::filesystem::path link = disk.Destination() / "aerosoft-crj";

    QVERIFY(
        service.engine.linking.Enable(Addon{addon, Manifest{}}, disk.Destination(), LinkType::Junction).Succeeded());
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

    Service service{.engine = Engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"}};
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

QTEST_APPLESS_MAIN(ImportOnRealDiskTest)

#include "tst_import_on_real_disk.moc"
