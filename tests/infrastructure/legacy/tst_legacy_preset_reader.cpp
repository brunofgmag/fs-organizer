#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "domain/support/PathUtils.h"
#include "infrastructure/legacy/LegacyPresetReader.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class LegacyPresetReaderTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ABareNameIsAnAddonToEnable();
        static void ALineWithAPathIsAFolderToEnable();
        static void ALineStartingWithAnAsteriskIsAnAddonToDisable();
        static void TheAsteriskIsStrippedTheWayTheOriginalStripsIt();
        static void EmptyAndRepeatedLinesAreDiscarded();
        static void TheNameOfThePresetComesFromTheFileName();
        static void AFileThatCannotBeOpenedIsNotTheSameAsAnEmptyPreset();
        static void AccentsSurviveTheFallbackEncoding();
        static void EveryPresetInTheFolderIsReadAndNothingElseIs();
        static void AFolderThatIsNotThereReadsNoPresets();
    };
}

namespace
{
    struct Storage
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return std::filesystem::path(directory.path().toStdString());
        }

        [[nodiscard]] std::filesystem::path Write(const std::string& name,
                                                  const std::vector<unsigned char>& bytes) const
        {
            const std::filesystem::path file = Root() / name;
            std::ofstream stream(file, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

            return file;
        }

        [[nodiscard]] std::filesystem::path WriteText(const std::string& name, const std::string& text) const
        {
            return Write(name, std::vector<unsigned char>(text.begin(), text.end()));
        }
    };
}

void LegacyPresetReaderTest::ABareNameIsAnAddonToEnable()
{
    const Storage storage;
    const std::optional<LegacyPresetSelection> read =
        ReadLegacyPreset(storage.WriteText("short hop.preset", "pmdg-777\r\nfenix-a320\r\n"));

    QVERIFY(read.has_value());
    QCOMPARE(read->enabledAddonNames.size(), std::size_t{2});
    QCOMPARE(read->enabledAddonNames.front(), std::string{"pmdg-777"});
    QVERIFY(read->enabledFolders.empty());
    QVERIFY(read->disabledAddonNames.empty());
}

void LegacyPresetReaderTest::ALineWithAPathIsAFolderToEnable()
{
    const Storage storage;
    const std::optional<LegacyPresetSelection> read =
        ReadLegacyPreset(storage.WriteText("a.preset", "d:\\msfs 2024\\aircrafts\r\npmdg-777\r\n"));

    QCOMPARE(read->enabledFolders.size(), std::size_t{1});
    QCOMPARE(ComparablePath(read->enabledFolders.front()), ComparablePath("D:/MSFS 2024/Aircrafts"));
    QCOMPARE(read->enabledAddonNames.size(), std::size_t{1});
}

void LegacyPresetReaderTest::ALineStartingWithAnAsteriskIsAnAddonToDisable()
{
    const Storage storage;
    const std::optional<LegacyPresetSelection> read =
        ReadLegacyPreset(storage.WriteText("a.preset", "pmdg-777\r\n*fenix-a320\r\n"));

    QCOMPARE(read->enabledAddonNames.size(), std::size_t{1});
    QCOMPARE(read->disabledAddonNames.size(), std::size_t{1});
    QCOMPARE(read->disabledAddonNames.front(), std::string{"fenix-a320"});
}

void LegacyPresetReaderTest::TheAsteriskIsStrippedTheWayTheOriginalStripsIt()
{
    const Storage storage;
    const std::optional<LegacyPresetSelection> read = ReadLegacyPreset(storage.WriteText("a.preset", "*a*b\r\n"));

    QCOMPARE(read->disabledAddonNames.front(), std::string{"ab"});
}

void LegacyPresetReaderTest::EmptyAndRepeatedLinesAreDiscarded()
{
    const Storage storage;
    const std::optional<LegacyPresetSelection> read =
        ReadLegacyPreset(storage.WriteText("a.preset", "pmdg-777\r\n\r\n   \r\nPMDG-777\r\npmdg-777\r\n"));

    QCOMPARE(read->enabledAddonNames.size(), std::size_t{1});
}

void LegacyPresetReaderTest::TheNameOfThePresetComesFromTheFileName()
{
    const Storage storage;

    QCOMPARE(ReadLegacyPreset(storage.WriteText("Short hop.preset", "pmdg-777\r\n"))->name, std::string{"Short hop"});
}

void LegacyPresetReaderTest::AFileThatCannotBeOpenedIsNotTheSameAsAnEmptyPreset()
{
    const Storage storage;

    QVERIFY(!ReadLegacyPreset(storage.Root() / "not there.preset").has_value());
    QVERIFY(ReadLegacyPreset(storage.WriteText("empty.preset", "")).has_value());
}

void LegacyPresetReaderTest::AccentsSurviveTheFallbackEncoding()
{
    const Storage storage;
    const std::vector<unsigned char> withBom{0xEF, 0xBB, 0xBF, 'a', 'v', 'i', 0xC3, 0xB5, 'e', 's'};

    QCOMPARE(ReadLegacyPreset(storage.Write("a.preset", withBom))->enabledAddonNames.front(),
             QStringLiteral("avi\u00F5es").toStdString());
}

void LegacyPresetReaderTest::EveryPresetInTheFolderIsReadAndNothingElseIs()
{
    const Storage storage;
    (void)storage.WriteText("b.preset", "pmdg-777\r\n");
    (void)storage.WriteText("a.PRESET", "fenix-a320\r\n");
    (void)storage.WriteText("AppliedPresets_History.bin", "");

    const std::vector<LegacyPresetSelection> presets = ReadLegacyPresetsIn(storage.Root());

    QCOMPARE(presets.size(), std::size_t{2});
    QCOMPARE(presets.front().name, std::string{"a"});
    QCOMPARE(presets.back().name, std::string{"b"});
}

void LegacyPresetReaderTest::AFolderThatIsNotThereReadsNoPresets()
{
    const Storage storage;

    QVERIFY(ReadLegacyPresetsIn(storage.Root() / "presets").empty());
}

QTEST_APPLESS_MAIN(LegacyPresetReaderTest)

#include "tst_legacy_preset_reader.moc"
