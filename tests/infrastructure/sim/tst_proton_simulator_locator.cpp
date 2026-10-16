#include <QtTest/QtTest>

#include <filesystem>
#include <string>
#include <vector>

#include "infrastructure/sim/ProtonPathMapper.h"
#include "infrastructure/sim/ProtonSimulatorLocator.h"
#include "infrastructure/sim/ProtonUserCfgLocations.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/ProtonTree.h"

namespace
{
    class ProtonSimulatorLocatorTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryPrefixIsReadThroughItsOwnDrives();
        static void TheDestinationFoldersAreFoundWhateverTheirCase();
        static void OnlyTheDestinationFoldersThatExistAreListed();
        static void TheOfficialFolderIsNotADestination();
        static void APrefixWithoutAUserConfigIsNotACandidate();
        static void AUserConfigWithoutThePackagesPathIsNotACandidate();
    };

    constexpr auto kRoaming2020 = "drive_c/users/steamuser/AppData/Roaming/Microsoft Flight Simulator";
    constexpr auto kRoaming2024 = "drive_c/users/steamuser/AppData/Roaming/Microsoft Flight Simulator 2024";

    struct Machine
    {
        Machine()
        {
            prefix2020 = tree.AddPrefix(tree.SteamRoot(), ProtonTree::kMsfs2020);
            prefix2024 = tree.AddPrefix(library2024, ProtonTree::kMsfs2024);
            tree.Drive(prefix2020, 's', tree.SteamRoot() / "steamapps");
            tree.Drive(prefix2024, 's', library2024);
        }

        void WriteUserCfg(const std::filesystem::path& prefix, const char* roaming, const std::string& packages) const
        {
            static_cast<void>(tree.WriteText(prefix / roaming / "UserCfg.opt",
                                             "Version 66\n{Video\n\tAdapter \"NVIDIA GeForce RTX 4080\"\n}\n"
                                             "InstalledPackagesPath \""
                                                 + packages + "\"\n"));
        }

        [[nodiscard]] std::vector<ProtonPrefix> Prefixes() const
        {
            return {{.variant = SimulatorVariant::MSFS2020, .path = prefix2020},
                    {.variant = SimulatorVariant::MSFS2024, .path = prefix2024}};
        }

        [[nodiscard]] std::vector<SimulatorCandidate> Located() const
        {
            const ProtonPathMapper mapper(Prefixes());

            return ProtonSimulatorLocator(ProtonUserCfgLocations(Prefixes()), mapper).Locate();
        }

        ProtonTree tree;
        std::filesystem::path library2024 = tree.Home() / "Jogos" / "SteamLibrary";
        std::filesystem::path prefix2020;
        std::filesystem::path prefix2024;
    };
}

void ProtonSimulatorLocatorTest::EveryPrefixIsReadThroughItsOwnDrives()
{
    const Machine machine;
    const std::filesystem::path packages2020 = machine.prefix2020 / kRoaming2020 / "Packages";
    const std::filesystem::path packages2024 = machine.library2024 / "Packages 2024";
    std::filesystem::create_directories(packages2020 / "Community");
    std::filesystem::create_directories(packages2024 / "Community");
    machine.WriteUserCfg(machine.prefix2020, kRoaming2020,
                         R"(C:\users\steamuser\AppData\Roaming\Microsoft Flight Simulator\Packages)");
    machine.WriteUserCfg(machine.prefix2024, kRoaming2024, R"(S:\Packages 2024)");

    const std::vector<SimulatorCandidate> candidates = machine.Located();

    QCOMPARE(candidates.size(), std::size_t{2});
    QCOMPARE(candidates[0].variant, SimulatorVariant::MSFS2020);
    QCOMPARE(candidates[0].packagesPath, packages2020);
    QCOMPARE(candidates[0].destinations, std::vector<std::filesystem::path>{packages2020 / "Community"});
    QCOMPARE(candidates[1].variant, SimulatorVariant::MSFS2024);
    QCOMPARE(candidates[1].packagesPath, packages2024);
    QCOMPARE(candidates[1].destinations, std::vector<std::filesystem::path>{packages2024 / "Community"});
}

void ProtonSimulatorLocatorTest::TheDestinationFoldersAreFoundWhateverTheirCase()
{
    const Machine machine;
    const std::filesystem::path packages = machine.prefix2024 / kRoaming2024 / "Packages";
    std::filesystem::create_directories(packages / "community");
    std::filesystem::create_directories(packages / "COMMUNITY2024");
    machine.WriteUserCfg(machine.prefix2024, kRoaming2024,
                         R"(C:\USERS\steamuser\appdata\Roaming\Microsoft Flight Simulator 2024\PACKAGES)");

    const std::vector<SimulatorCandidate> candidates = machine.Located();

    QCOMPARE(candidates.size(), std::size_t{1});
    QCOMPARE(candidates[0].packagesPath, packages);
    QCOMPARE(candidates[0].destinations,
             (std::vector<std::filesystem::path>{packages / "community", packages / "COMMUNITY2024"}));
}

void ProtonSimulatorLocatorTest::OnlyTheDestinationFoldersThatExistAreListed()
{
    const Machine machine;
    const std::filesystem::path packages = machine.prefix2024 / kRoaming2024 / "Packages";
    std::filesystem::create_directories(packages / "Community2024");
    machine.WriteUserCfg(machine.prefix2024, kRoaming2024,
                         R"(C:\users\steamuser\AppData\Roaming\Microsoft Flight Simulator 2024\Packages)");

    const std::vector<SimulatorCandidate> candidates = machine.Located();

    QCOMPARE(candidates.size(), std::size_t{1});
    QCOMPARE(candidates[0].destinations, std::vector<std::filesystem::path>{packages / "Community2024"});
}

void ProtonSimulatorLocatorTest::TheOfficialFolderIsNotADestination()
{
    const Machine machine;
    const std::filesystem::path packages = machine.prefix2024 / kRoaming2024 / "Packages";
    std::filesystem::create_directories(packages / "Official");
    machine.WriteUserCfg(machine.prefix2024, kRoaming2024,
                         R"(C:\users\steamuser\AppData\Roaming\Microsoft Flight Simulator 2024\Packages)");

    const std::vector<SimulatorCandidate> candidates = machine.Located();

    QCOMPARE(candidates.size(), std::size_t{1});
    QVERIFY(candidates[0].destinations.empty());
}

void ProtonSimulatorLocatorTest::APrefixWithoutAUserConfigIsNotACandidate()
{
    const Machine machine;
    machine.WriteUserCfg(machine.prefix2024, kRoaming2024, R"(S:\Packages 2024)");

    const std::vector<SimulatorCandidate> candidates = machine.Located();

    QCOMPARE(candidates.size(), std::size_t{1});
    QCOMPARE(candidates[0].variant, SimulatorVariant::MSFS2024);
}

void ProtonSimulatorLocatorTest::AUserConfigWithoutThePackagesPathIsNotACandidate()
{
    const Machine machine;
    static_cast<void>(
        machine.tree.WriteText(machine.prefix2024 / kRoaming2024 / "UserCfg.opt", "Version 66\n{Video\n}\n"));

    QVERIFY(machine.Located().empty());
}

QTEST_APPLESS_MAIN(ProtonSimulatorLocatorTest)

#include "tst_proton_simulator_locator.moc"
