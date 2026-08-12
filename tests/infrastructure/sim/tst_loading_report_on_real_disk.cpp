#include <QtTest/QtTest>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "application/ports/LoadingReportSource.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/sim/LoadingReportLocations.h"
#include "infrastructure/sim/ProfileLoadingReport.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/TempFiles.h"

namespace
{
    class LoadingReportOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheReportIsFoundBesideTheUserCfgAndReadFromTheBytesOnDisk();
        static void AProfileWithoutAReportIsFoundWithoutOneInsteadOfFailing();
        static void ThePathTheLocatorSkippedLeavesTheSourceAnsweringNothing();
        static void AFileThatIsNotAReportAnswersNothingInsteadOfAnEmptyReport();
    };

    [[nodiscard]] std::string Fixture(const std::string& name)
    {
        std::ifstream file(std::filesystem::path(FSORG_FIXTURES_DIR) / name, std::ios::binary);

        return std::string(std::istreambuf_iterator(file), std::istreambuf_iterator<char>());
    }

    [[nodiscard]] std::vector<UserCfgLocation> PointingAt(const std::filesystem::path& userCfg)
    {
        return {{.variant = SimulatorVariant::MSFS2024, .configPath = userCfg}};
    }

    void LoadingReportOnRealDiskTest::TheReportIsFoundBesideTheUserCfgAndReadFromTheBytesOnDisk()
    {
        const TempFiles profile;
        const std::filesystem::path userCfg = profile.WriteText("UserCfg.opt", "InstalledPackagesPath \"D:\\\\\"\n");
        const std::filesystem::path written =
            profile.WriteText("Report-loading.toml", Fixture("simulator-report-loading.toml"));

        const WindowsFilesystemProbe filesystemProbe;
        const std::vector<LoadingReportLocation> found = LoadingReportLocations(PointingAt(userCfg), filesystemProbe);

        QCOMPARE(found.size(), std::size_t{1});
        QCOMPARE(LoadingReportOf(found, SimulatorVariant::MSFS2024), written);

        const ProfileLoadingReport source(filesystemProbe, LoadingReportOf(found, SimulatorVariant::MSFS2024));
        const std::optional<LoadingReport> read = source.LastReport();

        QVERIFY(read.has_value());
        QCOMPARE(read->modules.size(), std::size_t{13});
        QCOMPARE(read->packagesRegistered, std::size_t{12});
        QVERIFY(read->runAt.has_value());
    }

    void LoadingReportOnRealDiskTest::AProfileWithoutAReportIsFoundWithoutOneInsteadOfFailing()
    {
        const TempFiles profile;
        const std::filesystem::path userCfg = profile.WriteText("UserCfg.opt", "InstalledPackagesPath \"D:\\\\\"\n");

        const WindowsFilesystemProbe filesystemProbe;
        const std::vector<LoadingReportLocation> found = LoadingReportLocations(PointingAt(userCfg), filesystemProbe);

        QVERIFY(found.empty());
        QVERIFY(LoadingReportOf(found, SimulatorVariant::MSFS2024).empty());
    }

    void LoadingReportOnRealDiskTest::ThePathTheLocatorSkippedLeavesTheSourceAnsweringNothing()
    {
        const WindowsFilesystemProbe filesystemProbe;
        const ProfileLoadingReport source(filesystemProbe, {});

        QVERIFY(!source.LastReport().has_value());
    }

    void LoadingReportOnRealDiskTest::AFileThatIsNotAReportAnswersNothingInsteadOfAnEmptyReport()
    {
        const TempFiles profile;
        const std::filesystem::path written = profile.WriteText("Report-loading.toml", "not a report at all\n");

        const WindowsFilesystemProbe filesystemProbe;
        const ProfileLoadingReport source(filesystemProbe, written);

        QVERIFY(!source.LastReport().has_value());
    }

}

QTEST_MAIN(LoadingReportOnRealDiskTest)

#include "tst_loading_report_on_real_disk.moc"
