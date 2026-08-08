#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <fstream>

#include "application/ProfileService.h"
#include "domain/importing/ExternalSidecar.h"
#include "domain/importing/ImportEngine.h"
#include "domain/importing/ImportPaths.h"
#include "domain/journal/OperationLog.h"
#include "domain/profile/ExternalOrigins.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class ExternalImportOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheFolderTheOtherProgramOwnsReallyBecomesAJunctionIntoTheLibrary();
        static void TheOtherProgramReadsItsFilesThroughTheFolderItAlwaysUsed();
        static void WhatTheOtherProgramWritesThroughItsFolderLandsInTheLibrary();
        static void TheEntryInTheDestinationReallyPointsAtTheLibrary();
        static void TheRecordBesideTheAddonIsOnDiskAndBringsTheLibraryBackOnItsOwn();
        static void AnInterruptedImportPutsTheOtherProgramsFolderBackWithItsFiles();
        static void AJunctionIsNotAPhysicalDirectoryAndThatIsHowDivergenceIsSeen();
    };

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

        [[nodiscard]] std::filesystem::path VendorFolder() const
        {
            return Root() / "Vendor" / "MSFS" / "gsx-pro";
        }

        [[nodiscard]] std::filesystem::path Entry() const
        {
            return Destination() / "fsdreamteam-gsx-pro";
        }

        [[nodiscard]] std::filesystem::path Landing() const
        {
            return Category() / "fsdreamteam-gsx-pro";
        }

        void AddFile(const std::filesystem::path& file, const std::string& content) const
        {
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

    class ALinkServiceThatRefusesToCreate final : public LinkService
    {
    public:
        [[nodiscard]] LinkFailure
        CreateLink(const std::filesystem::path&, const std::filesystem::path&, const LinkType) override
        {
            return LinkFailure::CouldNotCreateLink;
        }

        [[nodiscard]] bool RemoveReparseNode(const std::filesystem::path& linkPath) override
        {
            return real_.RemoveReparseNode(linkPath);
        }

        [[nodiscard]] std::optional<std::filesystem::path>
        ReadLinkTarget(const std::filesystem::path& path) const override
        {
            return real_.ReadLinkTarget(path);
        }

    private:
        WindowsLinkService real_{};
    };

    std::string ContentsOfFile(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);

        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    void PutTheOtherProgramsAddonOnDisk(const Disk& disk)
    {
        std::filesystem::create_directories(disk.Category());
        std::filesystem::create_directories(disk.Destination());
        disk.AddFile(disk.VendorFolder() / "manifest.json", R"({"title": "GSX Pro", "package_version": "2.9.1"})");
        disk.AddFile(disk.VendorFolder() / "SimObjects" / "gsx.bgl", std::string(2048, 'g'));

        WindowsLinkService links;
        QCOMPARE(links.CreateLink(disk.Entry(), disk.VendorFolder(), LinkType::Junction), LinkFailure::None);
    }

    [[nodiscard]] ImportRequest RequestFor(const Disk& disk)
    {
        return ImportRequest{
            .source = disk.Entry(), .category = disk.Category(), .externalSource = disk.VendorFolder()};
    }

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
}

void ExternalImportOnRealDiskTest::TheFolderTheOtherProgramOwnsReallyBecomesAJunctionIntoTheLibrary()
{
    const Disk disk;
    PutTheOtherProgramsAddonOnDisk(disk);

    const Engine engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"};

    QCOMPARE(engine.engine.Import(disk.Profile(), RequestFor(disk), {}).Result(), FileResult::Completed);

    QVERIFY(engine.filesystemProbe.IsReparsePoint(disk.VendorFolder()));
    QCOMPARE(NormalizeReparseTarget(engine.linkService.ReadLinkTarget(disk.VendorFolder()).value()), disk.Landing());
    QVERIFY(!std::filesystem::exists(SwapSlotFor(disk.VendorFolder())));
    QCOMPARE(std::filesystem::file_size(disk.Landing() / "SimObjects" / "gsx.bgl"), std::uintmax_t{2048});
}

void ExternalImportOnRealDiskTest::TheOtherProgramReadsItsFilesThroughTheFolderItAlwaysUsed()
{
    const Disk disk;
    PutTheOtherProgramsAddonOnDisk(disk);

    const Engine engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"};

    QCOMPARE(engine.engine.Import(disk.Profile(), RequestFor(disk), {}).Result(), FileResult::Completed);

    QCOMPARE(ContentsOfFile(disk.VendorFolder() / "manifest.json"),
             std::string(R"({"title": "GSX Pro", "package_version": "2.9.1"})"));
}

void ExternalImportOnRealDiskTest::WhatTheOtherProgramWritesThroughItsFolderLandsInTheLibrary()
{
    const Disk disk;
    PutTheOtherProgramsAddonOnDisk(disk);

    const Engine engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"};

    QCOMPARE(engine.engine.Import(disk.Profile(), RequestFor(disk), {}).Result(), FileResult::Completed);

    disk.AddFile(disk.VendorFolder() / "SimObjects" / "gsx-update.bgl", "the vendor updated this");

    QCOMPARE(ContentsOfFile(disk.Landing() / "SimObjects" / "gsx-update.bgl"), std::string("the vendor updated this"));
}

void ExternalImportOnRealDiskTest::TheEntryInTheDestinationReallyPointsAtTheLibrary()
{
    const Disk disk;
    PutTheOtherProgramsAddonOnDisk(disk);

    const Engine engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"};

    QCOMPARE(engine.engine.Import(disk.Profile(), RequestFor(disk), {}).Result(), FileResult::Completed);

    QVERIFY(engine.filesystemProbe.IsReparsePoint(disk.Entry()));
    QCOMPARE(NormalizeReparseTarget(engine.linkService.ReadLinkTarget(disk.Entry()).value()), disk.Landing());
}

void ExternalImportOnRealDiskTest::TheRecordBesideTheAddonIsOnDiskAndBringsTheLibraryBackOnItsOwn()
{
    const Disk disk;
    PutTheOtherProgramsAddonOnDisk(disk);

    Engine engine{.journalFile = disk.Root() / "journal" / "operations.jsonl"};

    QCOMPARE(engine.engine.Import(disk.Profile(), RequestFor(disk), {}).Result(), FileResult::Completed);

    QVERIFY(std::filesystem::exists(ExternalSidecarPathFor(disk.Landing())));

    WindowsProcessProbe processProbe{std::vector<std::string>{}};
    JsonManifestParser manifestParser;
    FilesystemScanner catalog{manifestParser, engine.filesystemProbe};
    EntryClassifier classifier{engine.linkService, engine.filesystemProbe};
    FakeLibraryIdGenerator identities;
    const ProfileService profiles(catalog, engine.filesystemProbe, classifier, engine.linking, engine.log, identities,
                                  LinkType::Junction);

    const SimulatorProfile forgetful = disk.Profile();
    QVERIFY(forgetful.externalOrigins.empty());

    const std::vector<DestinationEntry> entries = profiles.Scan(forgetful).entries;

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().externalOrigin, disk.VendorFolder());
}

void ExternalImportOnRealDiskTest::AnInterruptedImportPutsTheOtherProgramsFolderBackWithItsFiles()
{
    const Disk disk;
    PutTheOtherProgramsAddonOnDisk(disk);

    ALinkServiceThatRefusesToCreate refusing;
    WindowsFilesystemProbe filesystemProbe;
    WindowsFileOperations files;
    const LinkingEngine linking{refusing, filesystemProbe};
    SystemClock clock;
    JsonlOperationJournal journal{disk.Root() / "journal" / "operations.jsonl"};
    const OperationLog log{journal, clock};
    const ImportEngine engine{filesystemProbe, files, linking, log, LinkType::Junction};

    QCOMPARE(engine.Import(disk.Profile(), RequestFor(disk), {}).Result(), FileResult::CouldNotCreateLink);

    QVERIFY(std::filesystem::is_directory(disk.VendorFolder()));
    QVERIFY(!filesystemProbe.IsReparsePoint(disk.VendorFolder()));
    QCOMPARE(std::filesystem::file_size(disk.VendorFolder() / "SimObjects" / "gsx.bgl"), std::uintmax_t{2048});
    QVERIFY(!std::filesystem::exists(SwapSlotFor(disk.VendorFolder())));
}

void ExternalImportOnRealDiskTest::AJunctionIsNotAPhysicalDirectoryAndThatIsHowDivergenceIsSeen()
{
    const Disk disk;
    PutTheOtherProgramsAddonOnDisk(disk);

    const WindowsFilesystemProbe filesystemProbe;

    QVERIFY(filesystemProbe.PhysicalDirectoryExists(disk.VendorFolder()));
    QVERIFY(!filesystemProbe.PhysicalDirectoryExists(disk.Entry()));
    QVERIFY(!filesystemProbe.PhysicalDirectoryExists(disk.Root() / "Vendor" / "MSFS" / "never-installed"));
    QVERIFY(!filesystemProbe.PhysicalDirectoryExists(disk.VendorFolder() / "manifest.json"));
}

QTEST_APPLESS_MAIN(ExternalImportOnRealDiskTest)

#include "tst_external_import_on_real_disk.moc"
