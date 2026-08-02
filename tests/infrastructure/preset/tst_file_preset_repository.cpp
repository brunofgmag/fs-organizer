#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "infrastructure/preset/FilePresetRepository.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class FilePresetRepositoryTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void APresetSurvivesTheRoundTripWithBothActions();
        static void APresetOfOneProfileDoesNotShowUpInAnother();
        static void RenamingKeepsTheEntriesAndDropsTheOldName();
        static void RenamingAPresetOntoItsOwnNameKeepsIt();
        static void ARenameTheDiskRefusesLeavesThePresetWhereItWas();
        static void SavingSaysWhetherThePresetLanded();
        static void RemovingDropsThePresetFromTheList();
        static void ANameThatClimbsOutOfThePresetRootIsRefusedInsteadOfWritten();
        static void AProfileIdThatClimbsOutOfThePresetRootNeverReachesAnotherFolder();
        static void TheListingCarriesWhenThePresetFileWasLastWritten();
    };
}

namespace
{
    struct Storage
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return std::filesystem::path(directory.path().toStdString()) / "presets";
        }
    };

    Preset ShortFlight()
    {
        Preset preset;
        preset.name = "Voo curto";
        preset.entries = {PresetEntry{.addonId = AddonId{.libraryId = "library-1", .folderName = "aerosoft-crj"},
                                      .action = PresetAction::Enable},
                          PresetEntry{.addonId = AddonId{.libraryId = "library-2", .folderName = "scenery-z"},
                                      .action = PresetAction::Disable}};

        return preset;
    }
}

void FilePresetRepositoryTest::APresetSurvivesTheRoundTripWithBothActions()
{
    const Storage storage;

    FilePresetRepository repository(storage.Root());
    QVERIFY(repository.Save("msfs2024", ShortFlight()));

    const std::optional<Preset> read = FilePresetRepository(storage.Root()).Load("msfs2024", "Voo curto");

    QVERIFY(read.has_value());
    QCOMPARE(QString::fromStdString(read->name), QString{"Voo curto"});
    QCOMPARE(read->entries.size(), std::size_t{2});
    QCOMPARE(QString::fromStdString(read->entries.front().addonId.libraryId), QString{"library-1"});
    QCOMPARE(QString::fromStdString(read->entries.front().addonId.folderName), QString{"aerosoft-crj"});
    QCOMPARE(read->entries.front().action, PresetAction::Enable);
    QCOMPARE(read->entries.back().action, PresetAction::Disable);
}

void FilePresetRepositoryTest::APresetOfOneProfileDoesNotShowUpInAnother()
{
    const Storage storage;

    FilePresetRepository repository(storage.Root());
    QVERIFY(repository.Save("msfs2024", ShortFlight()));

    Preset other;
    other.name = "Treino";
    QVERIFY(repository.Save("msfs2020", other));

    const std::vector<PresetListing> ofTheNewer = repository.List("msfs2024");

    QCOMPARE(ofTheNewer.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(ofTheNewer.front().name), QString{"Voo curto"});
    QVERIFY(!repository.Load("msfs2020", "Voo curto").has_value());
}

void FilePresetRepositoryTest::RenamingKeepsTheEntriesAndDropsTheOldName()
{
    const Storage storage;

    FilePresetRepository repository(storage.Root());
    QVERIFY(repository.Save("msfs2024", ShortFlight()));

    QVERIFY(repository.Rename("msfs2024", "Voo curto", "Voo longo"));

    const std::optional<Preset> read = repository.Load("msfs2024", "Voo longo");

    QVERIFY(read.has_value());
    QCOMPARE(QString::fromStdString(read->name), QString{"Voo longo"});
    QCOMPARE(read->entries.size(), std::size_t{2});
    QVERIFY(!repository.Load("msfs2024", "Voo curto").has_value());
    QCOMPARE(repository.List("msfs2024").size(), std::size_t{1});
}

void FilePresetRepositoryTest::RenamingAPresetOntoItsOwnNameKeepsIt()
{
    const Storage storage;

    FilePresetRepository repository(storage.Root());
    QVERIFY(repository.Save("msfs2024", ShortFlight()));

    QVERIFY(repository.Rename("msfs2024", "Voo curto", "Voo curto"));

    const std::optional<Preset> read = repository.Load("msfs2024", "Voo curto");

    QVERIFY(read.has_value());
    QCOMPARE(QString::fromStdString(read->name), QString{"Voo curto"});
    QCOMPARE(read->entries.size(), std::size_t{2});
    QCOMPARE(repository.List("msfs2024").size(), std::size_t{1});
}

void FilePresetRepositoryTest::ARenameTheDiskRefusesLeavesThePresetWhereItWas()
{
    const Storage storage;

    FilePresetRepository repository(storage.Root());
    QVERIFY(repository.Save("msfs2024", ShortFlight()));

    const std::string longerThanTheFilesystemAllows(300, 'p');

    QVERIFY(!repository.Rename("msfs2024", "Voo curto", longerThanTheFilesystemAllows));

    const std::optional<Preset> read = repository.Load("msfs2024", "Voo curto");

    QVERIFY(read.has_value());
    QCOMPARE(read->entries.size(), std::size_t{2});
    QCOMPARE(repository.List("msfs2024").size(), std::size_t{1});
}

void FilePresetRepositoryTest::SavingSaysWhetherThePresetLanded()
{
    const Storage storage;

    FilePresetRepository repository(storage.Root());

    QVERIFY(repository.Save("msfs2024", ShortFlight()));

    Preset unnameable = ShortFlight();
    unnameable.name = std::string(300, 'p');

    QVERIFY(!repository.Save("msfs2024", unnameable));
    QCOMPARE(repository.List("msfs2024").size(), std::size_t{1});
}

void FilePresetRepositoryTest::RemovingDropsThePresetFromTheList()
{
    const Storage storage;

    FilePresetRepository repository(storage.Root());
    QVERIFY(repository.Save("msfs2024", ShortFlight()));

    repository.Remove("msfs2024", "Voo curto");

    QVERIFY(repository.List("msfs2024").empty());
    QVERIFY(!repository.Load("msfs2024", "Voo curto").has_value());
}

void FilePresetRepositoryTest::ANameThatClimbsOutOfThePresetRootIsRefusedInsteadOfWritten()
{
    const Storage storage;

    FilePresetRepository repository(storage.Root());

    Preset escaping = ShortFlight();
    escaping.name = R"(..\..\escaped)";

    QVERIFY(!repository.Save("msfs2024", escaping));
    QVERIFY(!std::filesystem::exists(storage.Root().parent_path().parent_path() / "escaped.json"));
    QVERIFY(repository.List("msfs2024").empty());
}

void FilePresetRepositoryTest::AProfileIdThatClimbsOutOfThePresetRootNeverReachesAnotherFolder()
{
    const Storage storage;

    const std::filesystem::path victim = storage.Root().parent_path() / "victim.json";
    std::filesystem::create_directories(victim.parent_path());
    std::ofstream(victim, std::ios::binary) << "{}";

    QVERIFY(std::filesystem::exists(victim));

    FilePresetRepository repository(storage.Root());
    repository.Remove("..", "victim");

    QVERIFY(std::filesystem::exists(victim));
}

void FilePresetRepositoryTest::TheListingCarriesWhenThePresetFileWasLastWritten()
{
    const Storage storage;

    FilePresetRepository repository(storage.Root());
    QVERIFY(repository.Save("msfs2024", ShortFlight()));

    const std::filesystem::path file = storage.Root() / "msfs2024" / "Voo curto.json";
    QVERIFY(std::filesystem::exists(file));

    constexpr auto twoDays = std::chrono::hours{48};
    std::filesystem::last_write_time(file, std::filesystem::file_time_type::clock::now() - twoDays);

    const std::vector<PresetListing> listings = repository.List("msfs2024");

    QCOMPARE(listings.size(), std::size_t{1});
    QVERIFY(listings.front().writtenAt.has_value());

    const auto ago = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now()
                                                                      - *listings.front().writtenAt);

    QVERIFY2(std::chrono::abs(ago - twoDays) < std::chrono::seconds{5},
             qPrintable(QStringLiteral("reported %1 s ago").arg(ago.count())));
}

QTEST_MAIN(FilePresetRepositoryTest)

#include "tst_file_preset_repository.moc"
