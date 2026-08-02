#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "infrastructure/sim/WindowsSimulatorLocator.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class WindowsSimulatorLocatorTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryCandidateIsReportedNotJustTheFirst();
        static void OnlyTheDestinationFoldersThatExistAreEnumerated();
    };
}

namespace
{
    struct Machine
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return {directory.path().toStdString()};
        }

        [[nodiscard]] std::filesystem::path AddUserCfg(const std::string& folder,
                                                       const std::filesystem::path& packagesPath) const
        {
            const std::filesystem::path configPath = Root() / folder / "UserCfg.opt";
            std::filesystem::create_directories(configPath.parent_path());

            std::ofstream file(configPath, std::ios::binary);
            file << "Version 66\n{Video\n\tAdapter \"NVIDIA GeForce RTX 4080\"\n}\n"
                 << "InstalledPackagesPath \"" << packagesPath.string() << "\"\n";

            return configPath;
        }

        void AddFolder(const std::string& relativePath) const
        {
            std::filesystem::create_directories(Root() / relativePath);
        }
    };
}

void WindowsSimulatorLocatorTest::EveryCandidateIsReportedNotJustTheFirst()
{
    const Machine machine;
    machine.AddFolder("packages-2020/Community");
    machine.AddFolder("packages-2024/Community");

    const std::vector<UserCfgLocation> locations{
        {.variant = SimulatorVariant::MSFS2020,
         .configPath = machine.AddUserCfg("store-2020", machine.Root() / "packages-2020")},
        {.variant = SimulatorVariant::MSFS2024, .configPath = machine.Root() / "never-installed" / "UserCfg.opt"},
        {.variant = SimulatorVariant::MSFS2024,
         .configPath = machine.AddUserCfg("steam-2024", machine.Root() / "packages-2024")},
    };

    const WindowsSimulatorLocator locator(locations);
    const std::vector<SimulatorCandidate> candidates = locator.Locate();

    QCOMPARE(candidates.size(), std::size_t{2});
    QCOMPARE(candidates[0].variant, SimulatorVariant::MSFS2020);
    QCOMPARE(candidates[0].packagesPath, machine.Root() / "packages-2020");
    QCOMPARE(candidates[1].variant, SimulatorVariant::MSFS2024);
    QCOMPARE(candidates[1].packagesPath, machine.Root() / "packages-2024");
}

void WindowsSimulatorLocatorTest::OnlyTheDestinationFoldersThatExistAreEnumerated()
{
    const Machine machine;
    machine.AddFolder("packages-2024/Community");
    machine.AddFolder("packages-2024/Community2024");
    machine.AddFolder("packages-2024/Official2024");
    machine.AddFolder("packages-2020/Community");

    const std::vector<UserCfgLocation> locations{
        {.variant = SimulatorVariant::MSFS2024,
         .configPath = machine.AddUserCfg("steam-2024", machine.Root() / "packages-2024")},
        {.variant = SimulatorVariant::MSFS2020,
         .configPath = machine.AddUserCfg("store-2020", machine.Root() / "packages-2020")},
    };

    const WindowsSimulatorLocator locator(locations);
    const std::vector<SimulatorCandidate> candidates = locator.Locate();

    QCOMPARE(candidates.size(), std::size_t{2});
    QCOMPARE(candidates[0].destinations.size(), std::size_t{2});
    QCOMPARE(candidates[0].destinations[0], machine.Root() / "packages-2024" / "Community");
    QCOMPARE(candidates[0].destinations[1], machine.Root() / "packages-2024" / "Community2024");
    QCOMPARE(candidates[1].destinations.size(), std::size_t{1});
    QCOMPARE(candidates[1].destinations[0], machine.Root() / "packages-2020" / "Community");
}

QTEST_APPLESS_MAIN(WindowsSimulatorLocatorTest)

#include "tst_windows_simulator_locator.moc"
