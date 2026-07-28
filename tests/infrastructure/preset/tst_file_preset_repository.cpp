#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <filesystem>
#include <string>

#include "infrastructure/preset/FilePresetRepository.h"
#include "tests/support/PathPrinting.h"

class FilePresetRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    static void APresetSurvivesTheRoundTripWithBothActions();
    static void APresetOfOneProfileDoesNotShowUpInAnother();
    static void RenamingKeepsTheEntriesAndDropsTheOldName();
    static void ARenameTheDiskRefusesLeavesThePresetWhereItWas();
    static void SavingSaysWhetherThePresetLanded();
    static void RemovingDropsThePresetFromTheList();
};

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
        preset.entries = {PresetEntry{AddonId{"library-1", "aerosoft-crj"}, PresetAction::Enable},
                          PresetEntry{AddonId{"library-2", "scenery-z"}, PresetAction::Disable}};

        return preset;
    }
}

void FilePresetRepositoryTest::APresetSurvivesTheRoundTripWithBothActions()
{
    const Storage storage;

    FilePresetRepository repository(storage.Root());
    repository.Save("msfs2024", ShortFlight());

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
    repository.Save("msfs2024", ShortFlight());

    Preset other;
    other.name = "Treino";
    repository.Save("msfs2020", other);

    const std::vector<std::string> ofTheNewer = repository.List("msfs2024");

    QCOMPARE(ofTheNewer.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(ofTheNewer.front()), QString{"Voo curto"});
    QVERIFY(!repository.Load("msfs2020", "Voo curto").has_value());
}

void FilePresetRepositoryTest::RenamingKeepsTheEntriesAndDropsTheOldName()
{
    const Storage storage;

    FilePresetRepository repository(storage.Root());
    repository.Save("msfs2024", ShortFlight());

    repository.Rename("msfs2024", "Voo curto", "Voo longo");

    const std::optional<Preset> read = repository.Load("msfs2024", "Voo longo");

    QVERIFY(read.has_value());
    QCOMPARE(QString::fromStdString(read->name), QString{"Voo longo"});
    QCOMPARE(read->entries.size(), std::size_t{2});
    QVERIFY(!repository.Load("msfs2024", "Voo curto").has_value());
    QCOMPARE(repository.List("msfs2024").size(), std::size_t{1});
}

void FilePresetRepositoryTest::ARenameTheDiskRefusesLeavesThePresetWhereItWas()
{
    const Storage storage;

    FilePresetRepository repository(storage.Root());
    repository.Save("msfs2024", ShortFlight());

    const std::string longerThanTheFilesystemAllows(300, 'p');

    repository.Rename("msfs2024", "Voo curto", longerThanTheFilesystemAllows);

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
    repository.Save("msfs2024", ShortFlight());

    repository.Remove("msfs2024", "Voo curto");

    QVERIFY(repository.List("msfs2024").empty());
    QVERIFY(!repository.Load("msfs2024", "Voo curto").has_value());
}

QTEST_MAIN(FilePresetRepositoryTest)

#include "tst_file_preset_repository.moc"
