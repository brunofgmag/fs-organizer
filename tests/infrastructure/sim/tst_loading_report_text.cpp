#include <QtTest/QtTest>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

#include "application/ports/LoadingReportSource.h"
#include "infrastructure/sim/LoadingReportText.h"

namespace
{
    class LoadingReportTextTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheRealReportGivesEveryModuleItsNamePackageAndMemory();
        static void TheRealReportCountsThePackagesTheSimulatorRegistered();
        static void TheRealReportCarriesTheInstantTheRunWrote();
        static void TheColumnsAreFoundByTheirNameAndNotByTheirPosition();
        static void AModuleWhoseMemoryIsNotANumberIsListedWithoutOne();
        static void TheGenerationPrefixComesOffForMatchingAndStaysOnForShowing();
        static void AReportWithoutModulesStillCountsThePackages();
        static void TextThatIsNotAReportCarriesNothing();
    };

    [[nodiscard]] std::string Fixture(const std::string& name)
    {
        std::ifstream file(std::filesystem::path(FSORG_FIXTURES_DIR) / name, std::ios::binary);

        return std::string(std::istreambuf_iterator(file), std::istreambuf_iterator<char>());
    }

    [[nodiscard]] LoadingReport TheRealReport()
    {
        return LoadingReportFrom(Fixture("simulator-report-loading.toml"));
    }

    void LoadingReportTextTest::TheRealReportGivesEveryModuleItsNamePackageAndMemory()
    {
        const LoadingReport report = TheRealReport();

        QCOMPARE(report.modules.size(), std::size_t{13});
        QCOMPARE(report.modules.front().moduleName, std::string("gsx-integrator-commbus.wasm"));
        QCOMPARE(report.modules.front().packageName, std::string("gsx-integrator-commbus"));
        QCOMPARE(report.modules.front().memoryBytes, std::optional<std::uintmax_t>{131072});
        QCOMPARE(report.modules.back().moduleName, std::string("noolaero-vdgs.wasm"));
        QCOMPARE(report.modules.back().memoryBytes, std::optional<std::uintmax_t>{14352384});
    }

    void LoadingReportTextTest::TheRealReportCountsThePackagesTheSimulatorRegistered()
    {
        QCOMPARE(TheRealReport().packagesRegistered, std::size_t{12});
    }

    void LoadingReportTextTest::TheRealReportCarriesTheInstantTheRunWrote()
    {
        using namespace std::chrono;

        const sys_seconds wroteAt = sys_days{2026y / August / 8d} + 18h + 41min + 21s;
        const std::optional<system_clock::time_point> ran = TheRealReport().runAt;

        QVERIFY(ran.has_value());
        QCOMPARE(floor<seconds>(*ran), wroteAt);
    }

    void LoadingReportTextTest::TheColumnsAreFoundByTheirNameAndNotByTheirPosition()
    {
        const LoadingReport report =
            LoadingReportFrom("[Wasm_Modules]\n"
                              "Format=\"Handle,PackageName,MemorySize,DebugName,Status\"\n"
                              "0=[1,\"p42-util-flow-pro\",327680,\"flowmodule.wasm\",\"Ready\"]\n");

        QCOMPARE(report.modules.size(), std::size_t{1});
        QCOMPARE(report.modules.front().moduleName, std::string("flowmodule.wasm"));
        QCOMPARE(report.modules.front().packageName, std::string("p42-util-flow-pro"));
        QCOMPARE(report.modules.front().memoryBytes, std::optional<std::uintmax_t>{327680});
    }

    void LoadingReportTextTest::AModuleWhoseMemoryIsNotANumberIsListedWithoutOne()
    {
        const LoadingReport report = LoadingReportFrom("[Wasm_Modules]\n"
                                                       "Format=\"Handle,DebugName,PackageName,MemorySize,Status\"\n"
                                                       "0=[1,\"quiet.wasm\",\"quiet-package\",\"-\",\"Ready\"]\n");

        QCOMPARE(report.modules.size(), std::size_t{1});
        QCOMPARE(report.modules.front().moduleName, std::string("quiet.wasm"));
        QVERIFY(!report.modules.front().memoryBytes.has_value());
    }

    void LoadingReportTextTest::TheGenerationPrefixComesOffForMatchingAndStaysOnForShowing()
    {
        const LoadingReport report =
            LoadingReportFrom("[Wasm_Modules]\n"
                              "Format=\"Handle,DebugName,PackageName,MemorySize\"\n"
                              "0=[1,\"light.wasm\",\"communityfs20-xmd11_light_mod_fs24\",4096]\n");

        QCOMPARE(report.modules.size(), std::size_t{1});
        QCOMPARE(report.modules.front().packageName, std::string("communityfs20-xmd11_light_mod_fs24"));
        QCOMPARE(report.modules.front().packageFolderName, std::string("xmd11_light_mod_fs24"));
    }

    void LoadingReportTextTest::AReportWithoutModulesStillCountsThePackages()
    {
        const LoadingReport report = LoadingReportFrom("[FlightSimulator_Packages]\n"
                                                       "one=[\"manifest=1\",\"Community\"]\n"
                                                       "two=[\"manifest=1\",\"Market\"]\n");

        QVERIFY(report.modules.empty());
        QCOMPARE(report.packagesRegistered, std::size_t{2});
    }

    void LoadingReportTextTest::TextThatIsNotAReportCarriesNothing()
    {
        const LoadingReport report = LoadingReportFrom("this file is not a report at all\n");

        QVERIFY(report.modules.empty());
        QCOMPARE(report.packagesRegistered, std::size_t{0});
        QVERIFY(!report.runAt.has_value());
    }
}

QTEST_MAIN(LoadingReportTextTest)

#include "tst_loading_report_text.moc"
