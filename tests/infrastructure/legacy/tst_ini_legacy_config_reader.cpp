#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "domain/support/PathUtils.h"
#include "infrastructure/legacy/IniLegacyConfigReader.h"

class IniLegacyConfigReaderTest : public QObject
{
    class IniLegacyConfigReaderTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheRepeatedKeyBecomesEveryAddonPathInOrder();
        static void TheSingleValuedKeysAreRead();
        static void ATrailingSeparatorIsTrimmedButARootKeepsIts();
        static void LinesThatAreNotKeyValueAreIgnored();
        static void TheShapeOfTheRealFileReadsWhole();
        static void AFileThatCannotBeOpenedIsNotTheSameAsAFileWithoutKeys();
        static void AByteOrderMarkWinsOverTheFallbackEncoding();
        static void AccentsSurviveTheFallbackEncodingWhenThereIsNoBom();
        static void AUtf16ByteOrderMarkIsHonouredInsteadOfDecodedAsBytes();
        static void BytesThatTheEncodingCannotDecodeAreRefusedInsteadOfReplaced();
        static void TextWithoutAnyBomKeepsEveryCharacter();
    };
}

namespace
{
    struct Storage
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Write(const std::string& name,
                                                  const std::vector<unsigned char>& bytes) const
        {
            const std::filesystem::path file = std::filesystem::path(directory.path().toStdString()) / name;
            std::ofstream stream(file, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            stream.close();

            return file;
        }

        [[nodiscard]] std::filesystem::path WriteText(const std::string& name, const std::string& text) const
        {
            return Write(name, std::vector<unsigned char>(text.begin(), text.end()));
        }
    };

    std::vector<unsigned char> WithUtf8Bom(const std::string& text)
    {
        std::vector<unsigned char> bytes{0xEF, 0xBB, 0xBF};
        bytes.insert(bytes.end(), text.begin(), text.end());

        return bytes;
    }

    constexpr auto kReferenceIni = "MyAddons_Path=D:\\MSFS 2024\\Aircraft Mods\n"
                                   "MyAddons_Path=D:\\MSFS 2024\\Aircrafts\n"
                                   "MyAddons_Path=D:\\MSFS 2024\\Liveries\n"
                                   "MSFSCommunity_Path=e:\\flight simulator 2024\\community\n"
                                   "Presets_Path=c:\\programdata\\msfs addons linker 2024\\presets\\\n"
                                   "Link_Type=J\n"
                                   "Disable_AdminCheck=True\n";
}

void IniLegacyConfigReaderTest::TheRepeatedKeyBecomesEveryAddonPathInOrder()
{
    const Storage storage;
    const std::optional<LegacyInstallation> read = ReadLegacyIni(storage.WriteText("a.ini", kReferenceIni));

    QVERIFY(read.has_value());
    QCOMPARE(read->addonPaths.size(), std::size_t{3});
    QCOMPARE(ComparablePath(read->addonPaths[0]), ComparablePath("D:/MSFS 2024/Aircraft Mods"));
    QCOMPARE(ComparablePath(read->addonPaths[2]), ComparablePath("D:/MSFS 2024/Liveries"));
}

void IniLegacyConfigReaderTest::TheSingleValuedKeysAreRead()
{
    const Storage storage;
    const std::optional<LegacyInstallation> read = ReadLegacyIni(storage.WriteText("a.ini", kReferenceIni));

    QCOMPARE(ComparablePath(read->communityPath), ComparablePath("e:/flight simulator 2024/community"));
    QCOMPARE(ComparablePath(read->presetsPath), ComparablePath("c:/programdata/msfs addons linker 2024/presets"));
    QCOMPARE(read->linkType, std::string{"J"});
}

void IniLegacyConfigReaderTest::ATrailingSeparatorIsTrimmedButARootKeepsIts()
{
    const Storage storage;
    const std::optional<LegacyInstallation> read = ReadLegacyIni(storage.WriteText(
        "trailing.ini",
        "Presets_Path=c:\\programdata\\presets\\\nMSFSCommunity_Path=e:\\\nMyAddons_Path=D:\\A\\B\\\n"));

    QCOMPARE(ComparablePath(read->presetsPath), ComparablePath("c:/programdata/presets"));
    QCOMPARE(ComparablePath(read->communityPath), std::string{"e:/"});
    QCOMPARE(ComparablePath(read->addonPaths[0]), ComparablePath("D:/A/B"));
}

void IniLegacyConfigReaderTest::LinesThatAreNotKeyValueAreIgnored()
{
    const Storage storage;
    const std::optional<LegacyInstallation> read = ReadLegacyIni(
        storage.WriteText("noise.ini", "\n[Section]\njunk without an equals\nMyAddons_Path=D:\\A\\B\n\n"));

    QCOMPARE(read->addonPaths.size(), std::size_t{1});
    QCOMPARE(ComparablePath(read->addonPaths[0]), ComparablePath("D:/A/B"));
}

void IniLegacyConfigReaderTest::TheShapeOfTheRealFileReadsWhole()
{
    const Storage storage;
    const std::vector<unsigned char> bytes =
        WithUtf8Bom("MyAddons_Path=D:\\MSFS 2024\\Aircraft Mods\r\n"
                    "MyAddons_Path=D:\\MSFS 2024\\Aircrafts\r\n"
                    "MSFSCommunity_Path=e:\\flight simulator 2024\\community\r\n"
                    "Presets_Path=c:\\programdata\\msfs addons linker 2024\\presets\\\r\n"
                    "Link_Type=J\r\n"
                    "Color_Folders_Disabled=16777215\r\n");

    const std::optional<LegacyInstallation> read = ReadLegacyIni(storage.Write("real.ini", bytes));

    QCOMPARE(read->addonPaths.size(), std::size_t{2});
    QCOMPARE(ComparablePath(read->addonPaths[0]), ComparablePath("D:/MSFS 2024/Aircraft Mods"));
    QCOMPARE(ComparablePath(read->communityPath), ComparablePath("e:/flight simulator 2024/community"));
    QCOMPARE(ComparablePath(read->presetsPath), ComparablePath("c:/programdata/msfs addons linker 2024/presets"));
    QCOMPARE(read->linkType, std::string{"J"});
}

void IniLegacyConfigReaderTest::AFileThatCannotBeOpenedIsNotTheSameAsAFileWithoutKeys()
{
    const Storage storage;
    const std::filesystem::path missing = std::filesystem::path(storage.directory.path().toStdString()) / "missing.ini";

    QVERIFY(!ReadLegacyIni(missing).has_value());
    QVERIFY(ReadLegacyIni(storage.WriteText("keyless.ini", "Disable_AdminCheck=True\n")).has_value());
}

void IniLegacyConfigReaderTest::AByteOrderMarkWinsOverTheFallbackEncoding()
{
    const std::optional<QString> text = DecodeLegacyText(QByteArray("\xEF\xBB\xBF"
                                                                    "Avi\xC3\xB5"
                                                                    "es"),
                                                         QStringDecoder::Latin1);

    QVERIFY(text.has_value());
    QCOMPARE(*text, QStringLiteral("Avi\u00F5es"));
}

void IniLegacyConfigReaderTest::AccentsSurviveTheFallbackEncodingWhenThereIsNoBom()
{
    const std::optional<QString> text = DecodeLegacyText(QByteArray("Avi\xF5"
                                                                    "es"),
                                                         QStringDecoder::Latin1);

    QVERIFY(text.has_value());
    QCOMPARE(*text, QStringLiteral("Avi\u00F5es"));
}

void IniLegacyConfigReaderTest::AUtf16ByteOrderMarkIsHonouredInsteadOfDecodedAsBytes()
{
    const QByteArray bytes = QByteArray("\xFF\xFE", 2) + QByteArray("A\0v\0i\0\xF5\0e\0s\0", 12);
    const std::optional<QString> text = DecodeLegacyText(bytes, QStringDecoder::Latin1);

    QVERIFY(text.has_value());
    QCOMPARE(*text, QStringLiteral("Avi\u00F5es"));
}

void IniLegacyConfigReaderTest::BytesThatTheEncodingCannotDecodeAreRefusedInsteadOfReplaced()
{
    QVERIFY(!DecodeLegacyText(QByteArray("Avi\xF5"
                                         "es"),
                              QStringDecoder::Utf8)
                 .has_value());
}

void IniLegacyConfigReaderTest::TextWithoutAnyBomKeepsEveryCharacter()
{
    const std::optional<QString> text = DecodeLegacyText(QByteArray("MyAddons_Path=D:\\A"), QStringDecoder::System);

    QVERIFY(text.has_value());
    QCOMPARE(*text, QStringLiteral("MyAddons_Path=D:\\A"));
}

QTEST_APPLESS_MAIN(IniLegacyConfigReaderTest)

#include "tst_ini_legacy_config_reader.moc"
