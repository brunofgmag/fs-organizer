#include <QtTest/QtTest>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "domain/support/PathUtils.h"
#include "infrastructure/sim/StartupFileLocations.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/StdFilesystemProbe.h"
#include "tests/support/TempFiles.h"

namespace
{
    class StartupFileLocationsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheExactNameIsTakenAndTheImpostorsBesideItAreNot();
        static void TheLowerCaseGraphyOfTheOlderSimulatorIsFoundToo();
        static void TheNameComesBackWithTheGraphyTheDiskHasAndNotTheOneAskedFor();
        static void AStartupFileWithNoUserCfgBesideItIsNotAdopted();
        static void TheBackupTheAppWritesIsNotTakenForTheSimulatorsFile();
        static void TheFileOfEachVariantIsFoundBesideItsOwnUserCfg();
        static void TheProfileInUseIsAnsweredWithTheFileOfItsOwnVariant();
        static void AVariantWithNoFileOfItsOwnIsAnsweredWithNothingAndNotWithTheOtherOne();
    };

    constexpr std::string_view kImpostors[] = {
        "exe_backup_fsdt.xml",
        "exe_backup_mdx.xml",
        "exe.xml before installing noolaero-module-vdgs",
        "EXE.XML.ifbak",
        "exe.xml.Parallel42-Flow.20260608-110209.before-add.bak",
        "exe.xml.Parallel42-Flow.20260712-191826.before-add.bak",
        "exe.xml.Parallel42-Flow.20260712-195812.before-add.bak",
    };

    [[nodiscard]] std::filesystem::path
    FolderWith(const TempFiles& files, const std::string& folder, const std::vector<std::string>& names)
    {
        const std::filesystem::path base = files.Root() / PathFromUtf8(folder);
        std::filesystem::create_directories(base);

        for (const std::string& name : names)
        {
            std::ofstream(base / PathFromUtf8(name), std::ios::binary) << "\n";
        }

        return base;
    }
}

void StartupFileLocationsTest::TheExactNameIsTakenAndTheImpostorsBesideItAreNot()
{
    const TempFiles files;

    std::vector<std::string> names{"UserCfg.opt", "EXE.xml", "EXE.xml.fsorg-backup"};
    for (const std::string_view impostor : kImpostors)
    {
        names.emplace_back(impostor);
    }

    const std::filesystem::path base = FolderWith(files, "Microsoft Flight Simulator 2024", names);

    const StdFilesystemProbe probe;
    const std::vector<StartupFileLocation> found =
        StartupFileLocations({{.variant = SimulatorVariant::MSFS2024, .configPath = base / "UserCfg.opt"}}, probe);

    QCOMPARE(found.size(), std::size_t{1});
    QCOMPARE(ComparableFileName(found.front().filePath), std::string("exe.xml"));
    QCOMPARE(ComparablePath(found.front().filePath.parent_path()), ComparablePath(base));
}

void StartupFileLocationsTest::TheLowerCaseGraphyOfTheOlderSimulatorIsFoundToo()
{
    const TempFiles files;
    const std::filesystem::path base = FolderWith(files, "Microsoft Flight Simulator", {"UserCfg.opt", "exe.xml"});

    const StdFilesystemProbe probe;
    const std::vector<StartupFileLocation> found =
        StartupFileLocations({{.variant = SimulatorVariant::MSFS2020, .configPath = base / "UserCfg.opt"}}, probe);

    QCOMPARE(found.size(), std::size_t{1});
    QCOMPARE(ComparableFileName(found.front().filePath), std::string("exe.xml"));

    qInfo() << "the graphy taken was" << QString::fromStdString(AsUtf8(found.front().filePath.filename()));
}

void StartupFileLocationsTest::TheNameComesBackWithTheGraphyTheDiskHasAndNotTheOneAskedFor()
{
    const TempFiles files;

    const std::filesystem::path older = FolderWith(files, "Microsoft Flight Simulator", {"UserCfg.opt", "exe.xml"});
    const std::filesystem::path newer =
        FolderWith(files, "Microsoft Flight Simulator 2024", {"UserCfg.opt", "EXE.xml"});

    const StdFilesystemProbe probe;
    const std::vector<StartupFileLocation> found =
        StartupFileLocations({{.variant = SimulatorVariant::MSFS2020, .configPath = older / "UserCfg.opt"},
                              {.variant = SimulatorVariant::MSFS2024, .configPath = newer / "UserCfg.opt"}},
                             probe);

    QCOMPARE(found.size(), std::size_t{2});
    QCOMPARE(found[0].filePath.filename(), std::filesystem::path("exe.xml"));
    QCOMPARE(found[1].filePath.filename(), std::filesystem::path("EXE.xml"));
}

void StartupFileLocationsTest::AStartupFileWithNoUserCfgBesideItIsNotAdopted()
{
    const TempFiles files;
    const std::filesystem::path base = FolderWith(files, "Microsoft Flight Simulator 2024", {"exe.xml"});

    const StdFilesystemProbe probe;
    const std::vector<StartupFileLocation> found =
        StartupFileLocations({{.variant = SimulatorVariant::MSFS2024, .configPath = base / "UserCfg.opt"}}, probe);

    QVERIFY(found.empty());
}

void StartupFileLocationsTest::TheBackupTheAppWritesIsNotTakenForTheSimulatorsFile()
{
    const TempFiles files;
    const std::filesystem::path base =
        FolderWith(files, "Microsoft Flight Simulator 2024", {"UserCfg.opt", "EXE.xml.fsorg-backup"});

    const StdFilesystemProbe probe;
    const std::vector<StartupFileLocation> found =
        StartupFileLocations({{.variant = SimulatorVariant::MSFS2024, .configPath = base / "UserCfg.opt"}}, probe);

    QVERIFY(found.empty());
    QCOMPARE(BackupOfStartupFile(base / "EXE.xml").filename(), std::filesystem::path("EXE.xml.fsorg-backup"));
}

void StartupFileLocationsTest::TheFileOfEachVariantIsFoundBesideItsOwnUserCfg()
{
    const TempFiles files;
    const std::filesystem::path older = FolderWith(files, "Microsoft Flight Simulator", {"UserCfg.opt", "exe.xml"});
    const std::filesystem::path newer =
        FolderWith(files, "Microsoft Flight Simulator 2024", {"UserCfg.opt", "EXE.xml"});

    const StdFilesystemProbe probe;
    const std::vector<StartupFileLocation> found =
        StartupFileLocations({{.variant = SimulatorVariant::MSFS2020, .configPath = older / "UserCfg.opt"},
                              {.variant = SimulatorVariant::MSFS2024, .configPath = newer / "UserCfg.opt"}},
                             probe);

    QCOMPARE(found.size(), std::size_t{2});
    QCOMPARE(ComparablePath(found[0].filePath.parent_path()), ComparablePath(older));
    QVERIFY(found[0].variant == SimulatorVariant::MSFS2020);
    QCOMPARE(ComparablePath(found[1].filePath.parent_path()), ComparablePath(newer));
    QVERIFY(found[1].variant == SimulatorVariant::MSFS2024);
}

void StartupFileLocationsTest::TheProfileInUseIsAnsweredWithTheFileOfItsOwnVariant()
{
    const TempFiles files;
    const std::filesystem::path older = FolderWith(files, "Microsoft Flight Simulator", {"UserCfg.opt", "exe.xml"});
    const std::filesystem::path newer =
        FolderWith(files, "Microsoft Flight Simulator 2024", {"UserCfg.opt", "EXE.xml"});

    const StdFilesystemProbe probe;
    const std::vector<StartupFileLocation> found =
        StartupFileLocations({{.variant = SimulatorVariant::MSFS2020, .configPath = older / "UserCfg.opt"},
                              {.variant = SimulatorVariant::MSFS2024, .configPath = newer / "UserCfg.opt"}},
                             probe);

    QCOMPARE(ComparablePath(StartupFileOf(found, SimulatorVariant::MSFS2020).parent_path()), ComparablePath(older));
    QCOMPARE(ComparablePath(StartupFileOf(found, SimulatorVariant::MSFS2024).parent_path()), ComparablePath(newer));
}

void StartupFileLocationsTest::AVariantWithNoFileOfItsOwnIsAnsweredWithNothingAndNotWithTheOtherOne()
{
    const TempFiles files;
    const std::filesystem::path newer =
        FolderWith(files, "Microsoft Flight Simulator 2024", {"UserCfg.opt", "EXE.xml"});

    const StdFilesystemProbe probe;
    const std::vector<StartupFileLocation> found =
        StartupFileLocations({{.variant = SimulatorVariant::MSFS2024, .configPath = newer / "UserCfg.opt"}}, probe);

    QVERIFY(StartupFileOf(found, SimulatorVariant::MSFS2020).empty());
    QVERIFY(!StartupFileOf(found, SimulatorVariant::MSFS2024).empty());
}

QTEST_APPLESS_MAIN(StartupFileLocationsTest)

#include "tst_startup_file_locations.moc"
