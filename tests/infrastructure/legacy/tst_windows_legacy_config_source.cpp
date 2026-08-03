#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "domain/support/PathUtils.h"
#include "infrastructure/legacy/WindowsLegacyConfigSource.h"

namespace
{
    class WindowsLegacyConfigSourceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryInstallationFolderUnderProgramDataIsFound();
        static void AFolderOfAnotherProgramIsIgnored();
        static void AFolderWithoutTheConfigurationFileIsIgnored();
        static void AConfigurationThatCannotBeOpenedIsFoundWithoutItsContents();
        static void TheEntriesOfTheConfigurationArriveWhole();
        static void AProgramDataFolderThatIsNotThereFindsNothing();
        static void ThePresetsOfAnInstallationAreReadFromItsPresetsFolder();
    };
}

namespace
{
    constexpr auto kConfigFileName = "MSFS_Addons_Linker.ini";

    struct ProgramData
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return std::filesystem::path(directory.path().toStdString());
        }

        [[nodiscard]] std::filesystem::path Folder(const std::string& name) const
        {
            const std::filesystem::path folder = Root() / name;
            std::filesystem::create_directories(folder);

            return folder;
        }

        void WriteConfiguration(const std::string& name, const std::vector<unsigned char>& bytes) const
        {
            const std::filesystem::path file = Folder(name) / kConfigFileName;
            std::ofstream stream(file, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
    };

    std::vector<unsigned char> WithUtf8Bom(const std::string& text)
    {
        std::vector<unsigned char> bytes{0xEF, 0xBB, 0xBF};
        bytes.insert(bytes.end(), text.begin(), text.end());

        return bytes;
    }

    std::vector<unsigned char> ReferenceConfigurationOf2024()
    {
        return WithUtf8Bom("MyAddons_Path=D:\\MSFS 2024\\Aircraft Mods\r\n"
                           "MyAddons_Path=D:\\MSFS 2024\\Aircrafts\r\n"
                           "MSFSCommunity_Path=e:\\flight simulator 2024\\community\r\n"
                           "Link_Type=J\r\n");
    }
}

void WindowsLegacyConfigSourceTest::EveryInstallationFolderUnderProgramDataIsFound()
{
    const ProgramData programData;
    programData.WriteConfiguration("MSFS Addons Linker 2024", ReferenceConfigurationOf2024());
    programData.WriteConfiguration("MSFS Addons Linker", WithUtf8Bom("MyAddons_Path=D:\\MSFS 2020\\Aircrafts\r\n"));

    const std::vector<FoundLegacyInstallation> found = WindowsLegacyConfigSource(programData.Root()).Installations();

    QCOMPARE(found.size(), std::size_t{2});
    QCOMPARE(ComparablePath(found[0].folder), ComparablePath(programData.Root() / "MSFS Addons Linker"));
    QCOMPARE(ComparablePath(found[1].folder), ComparablePath(programData.Root() / "MSFS Addons Linker 2024"));
}

void WindowsLegacyConfigSourceTest::AFolderOfAnotherProgramIsIgnored()
{
    const ProgramData programData;
    programData.WriteConfiguration("MSFS Addons Linker 2024", ReferenceConfigurationOf2024());
    programData.WriteConfiguration("Orbx", ReferenceConfigurationOf2024());

    const std::vector<FoundLegacyInstallation> found = WindowsLegacyConfigSource(programData.Root()).Installations();

    QCOMPARE(found.size(), std::size_t{1});
    QCOMPARE(ComparablePath(found.front().folder), ComparablePath(programData.Root() / "MSFS Addons Linker 2024"));
}

void WindowsLegacyConfigSourceTest::AFolderWithoutTheConfigurationFileIsIgnored()
{
    const ProgramData programData;
    (void)programData.Folder("MSFS Addons Linker 2024");

    QVERIFY(WindowsLegacyConfigSource(programData.Root()).Installations().empty());
}

void WindowsLegacyConfigSourceTest::AConfigurationThatCannotBeOpenedIsFoundWithoutItsContents()
{
    const ProgramData programData;
    std::filesystem::create_directories(programData.Folder("MSFS Addons Linker 2024") / kConfigFileName);

    const std::vector<FoundLegacyInstallation> found = WindowsLegacyConfigSource(programData.Root()).Installations();

    QCOMPARE(found.size(), std::size_t{1});
    QVERIFY(!found.front().configuration.has_value());
}

void WindowsLegacyConfigSourceTest::TheEntriesOfTheConfigurationArriveWhole()
{
    const ProgramData programData;
    programData.WriteConfiguration("MSFS Addons Linker 2024", ReferenceConfigurationOf2024());

    const std::vector<FoundLegacyInstallation> found = WindowsLegacyConfigSource(programData.Root()).Installations();

    QVERIFY(found.front().configuration.has_value());
    QCOMPARE(found.front().configuration->addonPaths.size(), std::size_t{2});
    QCOMPARE(ComparablePath(found.front().configuration->addonPaths.front()),
             ComparablePath("D:/MSFS 2024/Aircraft Mods"));
}

void WindowsLegacyConfigSourceTest::AProgramDataFolderThatIsNotThereFindsNothing()
{
    const ProgramData programData;

    QVERIFY(WindowsLegacyConfigSource(programData.Root() / "not there").Installations().empty());
}

void WindowsLegacyConfigSourceTest::ThePresetsOfAnInstallationAreReadFromItsPresetsFolder()
{
    const ProgramData programData;
    const std::filesystem::path presets = programData.Folder("MSFS Addons Linker 2024") / "Presets";
    std::filesystem::create_directories(presets);
    std::ofstream(presets / "Voo curto.preset", std::ios::binary) << "pmdg-777\r\n";

    const std::vector<LegacyPresetSelection> read = WindowsLegacyConfigSource(programData.Root()).PresetsIn(presets);

    QCOMPARE(read.size(), std::size_t{1});
    QCOMPARE(read.front().name, std::string{"Voo curto"});
    QCOMPARE(read.front().enabledAddonNames.front(), std::string{"pmdg-777"});
}

QTEST_APPLESS_MAIN(WindowsLegacyConfigSourceTest)

#include "tst_windows_legacy_config_source.moc"
