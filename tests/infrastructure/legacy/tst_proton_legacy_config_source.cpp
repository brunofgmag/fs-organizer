#include <QtTest/QtTest>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "domain/support/PathUtils.h"
#include "infrastructure/legacy/ProtonLegacyConfigSource.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/ProtonTree.h"

namespace
{
    class ProtonLegacyConfigSourceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryInstallationFolderUnderTheProgramDataOfEveryPrefixIsFound();
        static void TheProgramDataFolderAndTheFileAreFoundWhateverTheirCase();
        static void AFolderOfAnotherProgramIsIgnored();
        static void AFolderWithoutTheConfigurationFileIsIgnored();
        static void AConfigurationThatCannotBeOpenedIsFoundWithoutItsContents();
        static void ThePathsOfTheConfigurationAreTranslatedThroughTheDrivesOfItsOwnPrefix();
        static void APathOnADriveThePrefixDoesNotHaveStaysAsWritten();
        static void ThePresetsOfAnInstallationAreReadFromItsPresetsFolder();
        static void TheFoldersOfAPresetAreTranslatedThroughThePrefixThatHoldsTheFolder();
        static void ThePresetsOfAFolderOutsideEveryPrefixAreReadAsTheyAre();
        static void APrefixWithoutProgramDataFindsNothing();
    };

    constexpr auto kConfigFileName = "MSFS_Addons_Linker.ini";

    struct Machine
    {
        Machine()
        {
            prefix2020 = tree.AddPrefix(tree.SteamRoot(), ProtonTree::kMsfs2020);
            prefix2024 = tree.AddPrefix(library2024, ProtonTree::kMsfs2024);
            tree.Drive(prefix2020, 'd', tree.Root() / "addons-of-2020");
            tree.Drive(prefix2024, 'd', tree.Root() / "addons-of-2024");
        }

        [[nodiscard]] std::filesystem::path Installation(const std::filesystem::path& prefix,
                                                         const std::string& name,
                                                         const std::string& configuration) const
        {
            const std::filesystem::path folder = prefix / "drive_c" / "ProgramData" / name;

            static_cast<void>(tree.WriteText(folder / kConfigFileName, configuration));

            return folder;
        }

        [[nodiscard]] ProtonLegacyConfigSource Source() const
        {
            return ProtonLegacyConfigSource({{.variant = SimulatorVariant::MSFS2020, .path = prefix2020},
                                             {.variant = SimulatorVariant::MSFS2024, .path = prefix2024}});
        }

        ProtonTree tree;
        std::filesystem::path library2024 = tree.Home() / "Jogos" / "SteamLibrary";
        std::filesystem::path prefix2020;
        std::filesystem::path prefix2024;
    };

    const std::string kReferenceConfiguration = "MyAddons_Path=D:\\MSFS 2024\\Aircraft Mods\r\n"
                                                "MyAddons_Path=D:\\MSFS 2024\\Aircrafts\r\n"
                                                "Presets_Path=C:\\ProgramData\\MSFS Addons Linker 2024\\Presets\r\n"
                                                "Link_Type=J\r\n";
}

void ProtonLegacyConfigSourceTest::EveryInstallationFolderUnderTheProgramDataOfEveryPrefixIsFound()
{
    const Machine machine;
    const std::filesystem::path of2024 =
        machine.Installation(machine.prefix2024, "MSFS Addons Linker 2024", kReferenceConfiguration);
    const std::filesystem::path of2020 = machine.Installation(machine.prefix2020, "MSFS Addons Linker", "");
    const std::filesystem::path second = machine.Installation(machine.prefix2024, "MSFS Addons Linker", "");

    const std::vector<FoundLegacyInstallation> found = machine.Source().Installations();

    QCOMPARE(found.size(), std::size_t{3});
    std::vector<std::string> folders;
    for (const FoundLegacyInstallation& installation : found)
    {
        folders.push_back(ComparablePath(installation.folder));
    }
    QVERIFY(std::ranges::is_sorted(folders));
    QVERIFY(std::ranges::count(folders, ComparablePath(of2024)) == 1);
    QVERIFY(std::ranges::count(folders, ComparablePath(of2020)) == 1);
    QVERIFY(std::ranges::count(folders, ComparablePath(second)) == 1);
}

void ProtonLegacyConfigSourceTest::TheProgramDataFolderAndTheFileAreFoundWhateverTheirCase()
{
    const Machine machine;
    const std::filesystem::path folder = machine.prefix2024 / "drive_c" / "PROGRAMDATA" / "msfs addons linker 2024";
    static_cast<void>(machine.tree.WriteText(folder / "msfs_addons_linker.INI", kReferenceConfiguration));

    const std::vector<FoundLegacyInstallation> found = machine.Source().Installations();

    QCOMPARE(found.size(), std::size_t{1});
    QCOMPARE(found[0].folder, folder);
    QVERIFY(found[0].configuration.has_value());
}

void ProtonLegacyConfigSourceTest::AFolderOfAnotherProgramIsIgnored()
{
    const Machine machine;
    static_cast<void>(machine.Installation(machine.prefix2024, "Orbx", kReferenceConfiguration));

    QVERIFY(machine.Source().Installations().empty());
}

void ProtonLegacyConfigSourceTest::AFolderWithoutTheConfigurationFileIsIgnored()
{
    const Machine machine;
    std::filesystem::create_directories(machine.prefix2024 / "drive_c" / "ProgramData" / "MSFS Addons Linker 2024");

    QVERIFY(machine.Source().Installations().empty());
}

void ProtonLegacyConfigSourceTest::AConfigurationThatCannotBeOpenedIsFoundWithoutItsContents()
{
    const Machine machine;
    std::filesystem::create_directories(machine.prefix2024 / "drive_c" / "ProgramData" / "MSFS Addons Linker 2024"
                                        / kConfigFileName);

    const std::vector<FoundLegacyInstallation> found = machine.Source().Installations();

    QCOMPARE(found.size(), std::size_t{1});
    QVERIFY(!found[0].configuration.has_value());
}

void ProtonLegacyConfigSourceTest::ThePathsOfTheConfigurationAreTranslatedThroughTheDrivesOfItsOwnPrefix()
{
    const Machine machine;
    std::filesystem::create_directories(machine.tree.Root() / "addons-of-2024" / "MSFS 2024" / "Aircraft Mods");
    std::filesystem::create_directories(machine.prefix2024 / "drive_c" / "ProgramData" / "MSFS Addons Linker 2024"
                                        / "Presets");
    static_cast<void>(machine.Installation(machine.prefix2024, "MSFS Addons Linker 2024", kReferenceConfiguration));
    static_cast<void>(machine.Installation(machine.prefix2020, "MSFS Addons Linker", kReferenceConfiguration));

    const std::vector<FoundLegacyInstallation> found = machine.Source().Installations();

    QCOMPARE(found.size(), std::size_t{2});

    const auto of2024 = std::ranges::find_if(found,
                                             [](const FoundLegacyInstallation& installation)
                                             {
                                                 return installation.folder.filename() == "MSFS Addons Linker 2024";
                                             });

    QVERIFY(of2024 != found.end() && of2024->configuration.has_value());
    QCOMPARE(of2024->configuration->addonPaths.size(), std::size_t{2});
    QCOMPARE(of2024->configuration->addonPaths[0],
             machine.tree.Root() / "addons-of-2024" / "MSFS 2024" / "Aircraft Mods");
    QCOMPARE(of2024->configuration->addonPaths[1], machine.tree.Root() / "addons-of-2024" / "MSFS 2024" / "Aircrafts");
    QCOMPARE(of2024->configuration->presetsPath,
             machine.prefix2024 / "drive_c" / "ProgramData" / "MSFS Addons Linker 2024" / "Presets");

    const auto of2020 = std::ranges::find_if(found,
                                             [](const FoundLegacyInstallation& installation)
                                             {
                                                 return installation.folder.filename() == "MSFS Addons Linker";
                                             });

    QVERIFY(of2020 != found.end() && of2020->configuration.has_value());
    QCOMPARE(of2020->configuration->addonPaths[0],
             machine.tree.Root() / "addons-of-2020" / "MSFS 2024" / "Aircraft Mods");
}

void ProtonLegacyConfigSourceTest::APathOnADriveThePrefixDoesNotHaveStaysAsWritten()
{
    const Machine machine;
    static_cast<void>(
        machine.Installation(machine.prefix2024, "MSFS Addons Linker 2024", "MyAddons_Path=X:\\Elsewhere\\Addons\r\n"));

    const std::vector<FoundLegacyInstallation> found = machine.Source().Installations();

    QCOMPARE(found.size(), std::size_t{1});
    QVERIFY(found[0].configuration.has_value());
    QCOMPARE(found[0].configuration->addonPaths.size(), std::size_t{1});
    QCOMPARE(AsUtf8(found[0].configuration->addonPaths[0]), std::string(R"(X:\Elsewhere\Addons)"));
}

void ProtonLegacyConfigSourceTest::ThePresetsOfAnInstallationAreReadFromItsPresetsFolder()
{
    const Machine machine;
    const std::filesystem::path presets =
        machine.Installation(machine.prefix2024, "MSFS Addons Linker 2024", "") / "Presets";
    static_cast<void>(machine.tree.WriteText(presets / "Voo curto.preset", "pmdg-777\r\n"));

    const std::vector<LegacyPresetSelection> read = machine.Source().PresetsIn(presets);

    QCOMPARE(read.size(), std::size_t{1});
    QCOMPARE(read[0].name, std::string{"Voo curto"});
    QCOMPARE(read[0].enabledAddonNames.front(), std::string{"pmdg-777"});
}

void ProtonLegacyConfigSourceTest::TheFoldersOfAPresetAreTranslatedThroughThePrefixThatHoldsTheFolder()
{
    const Machine machine;
    std::filesystem::create_directories(machine.tree.Root() / "addons-of-2020" / "Aircrafts" / "pmdg-777");
    std::filesystem::create_directories(machine.tree.Root() / "addons-of-2024" / "Aircrafts" / "pmdg-777");
    static_cast<void>(machine.Installation(machine.prefix2020, "MSFS Addons Linker", ""));
    const std::filesystem::path presets2024 =
        machine.Installation(machine.prefix2024, "MSFS Addons Linker 2024", "") / "Presets";
    static_cast<void>(machine.tree.WriteText(presets2024 / "Voo curto.preset", "D:\\Aircrafts\\PMDG-777\r\nfenix\r\n"));

    const std::vector<LegacyPresetSelection> read = machine.Source().PresetsIn(presets2024);

    QCOMPARE(read.size(), std::size_t{1});
    QCOMPARE(read[0].enabledFolders.size(), std::size_t{1});
    QCOMPARE(read[0].enabledFolders[0], machine.tree.Root() / "addons-of-2024" / "Aircrafts" / "pmdg-777");
    QCOMPARE(read[0].enabledAddonNames, std::vector<std::string>{"fenix"});
}

void ProtonLegacyConfigSourceTest::ThePresetsOfAFolderOutsideEveryPrefixAreReadAsTheyAre()
{
    const Machine machine;
    const std::filesystem::path presets = machine.tree.Root() / "elsewhere" / "Presets";
    static_cast<void>(machine.tree.WriteText(presets / "Voo curto.preset", "D:\\Aircrafts\\pmdg-777\r\n"));

    const std::vector<LegacyPresetSelection> read = machine.Source().PresetsIn(presets);

    QCOMPARE(read.size(), std::size_t{1});
    QCOMPARE(AsUtf8(read[0].enabledFolders.at(0)), std::string(R"(D:\Aircrafts\pmdg-777)"));
}

void ProtonLegacyConfigSourceTest::APrefixWithoutProgramDataFindsNothing()
{
    const Machine machine;

    QVERIFY(machine.Source().Installations().empty());
}

QTEST_APPLESS_MAIN(ProtonLegacyConfigSourceTest)

#include "tst_proton_legacy_config_source.moc"
