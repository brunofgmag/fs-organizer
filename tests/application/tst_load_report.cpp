#include <QtTest/QtTest>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "application/LoadReport.h"
#include "tests/support/PathPrinting.h"

namespace
{
    const std::filesystem::path kLibrary = "D:/MSFS 2024";
    const std::filesystem::path kFlow = kLibrary / "Utilities" / "p42-util-flow-pro";
    const std::filesystem::path kChasePlane = kLibrary / "Utilities" / "P42-Util-ChasePlane";

    class LoadReportTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AModuleWhosePackageIsOneOfYourAddonsSaysWhichFolderItIs();
        static void AModuleWhosePackageIsNotYoursIsListedByTheNameTheReportGave();
        static void TheMatchDoesNotCareAboutCase();
        static void NoReportLeavesTheSectionEmptyInsteadOfClaimingZeroes();
        static void TheHeaviestModuleComesFirstAndTheUnattributedComeLast();
        static void ThePackagesTheSimulatorRegisteredAreCarriedThroughWithTheirInstant();
    };

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    ProfileSnapshot SnapshotHolding(std::vector<TreeNode> addons)
    {
        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = std::move(addons);

        ProfileSnapshot snapshot;
        snapshot.libraries.push_back(std::move(library));

        return snapshot;
    }

    LoadedModule Module(const std::string& moduleName,
                        const std::string& packageName,
                        const std::optional<std::uintmax_t> memoryBytes)
    {
        return LoadedModule{.moduleName = moduleName,
                            .packageName = packageName,
                            .packageFolderName = packageName,
                            .memoryBytes = memoryBytes};
    }

    void LoadReportTest::AModuleWhosePackageIsOneOfYourAddonsSaysWhichFolderItIs()
    {
        const LoadingReport read{.modules = {Module("flowmodule.wasm", "p42-util-flow-pro", 327680)}};

        const LoadDiagnostics reported = ReportTheLoad(read, SnapshotHolding({AddonNode(kFlow)}));

        QCOMPARE(reported.modules.size(), std::size_t{1});
        QCOMPARE(reported.modules.front().addonUnderLibrary, std::filesystem::path("Utilities/p42-util-flow-pro"));
    }

    void LoadReportTest::AModuleWhosePackageIsNotYoursIsListedByTheNameTheReportGave()
    {
        const LoadingReport read{.modules = {Module("fsdt-msfs-bridge.wasm", "fsdreamteam-gsx-pro", 327680)}};

        const LoadDiagnostics reported = ReportTheLoad(read, SnapshotHolding({AddonNode(kFlow)}));

        QCOMPARE(reported.modules.size(), std::size_t{1});
        QCOMPARE(reported.modules.front().packageName, std::string("fsdreamteam-gsx-pro"));
        QVERIFY(reported.modules.front().addonUnderLibrary.empty());
    }

    void LoadReportTest::TheMatchDoesNotCareAboutCase()
    {
        const LoadingReport read{.modules = {Module("chaseplanemodule.wasm", "p42-util-chaseplane", 458752)}};

        const LoadDiagnostics reported = ReportTheLoad(read, SnapshotHolding({AddonNode(kChasePlane)}));

        QCOMPARE(reported.modules.size(), std::size_t{1});
        QCOMPARE(reported.modules.front().addonUnderLibrary, std::filesystem::path("Utilities/P42-Util-ChasePlane"));
    }

    void LoadReportTest::NoReportLeavesTheSectionEmptyInsteadOfClaimingZeroes()
    {
        const LoadDiagnostics reported = ReportTheLoad(std::nullopt, SnapshotHolding({AddonNode(kFlow)}));

        QVERIFY(!reported.reportWasRead);
        QVERIFY(reported.modules.empty());
        QCOMPARE(reported.packagesRegistered, std::size_t{0});
        QVERIFY(!reported.runAt.has_value());
    }

    void LoadReportTest::TheHeaviestModuleComesFirstAndTheUnattributedComeLast()
    {
        const LoadingReport read{.modules = {Module("quiet.wasm", "quiet-package", std::nullopt),
                                             Module("small.wasm", "small-package", 131072),
                                             Module("heavy.wasm", "heavy-package", 268763136)}};

        const LoadDiagnostics reported = ReportTheLoad(read, SnapshotHolding({}));

        QCOMPARE(reported.modules.size(), std::size_t{3});
        QCOMPARE(reported.modules.at(0).moduleName, std::string("heavy.wasm"));
        QCOMPARE(reported.modules.at(1).moduleName, std::string("small.wasm"));
        QCOMPARE(reported.modules.at(2).moduleName, std::string("quiet.wasm"));
    }

    void LoadReportTest::ThePackagesTheSimulatorRegisteredAreCarriedThroughWithTheirInstant()
    {
        using namespace std::chrono;

        const sys_seconds wroteAt = sys_days{2026y / August / 8d} + 18h + 41min + 21s;
        const LoadingReport read{.packagesRegistered = 264, .runAt = wroteAt};

        const LoadDiagnostics reported = ReportTheLoad(read, SnapshotHolding({}));

        QVERIFY(reported.reportWasRead);
        QCOMPARE(reported.packagesRegistered, std::size_t{264});
        QCOMPARE(reported.runAt, std::optional<system_clock::time_point>{wroteAt});
    }
}

QTEST_MAIN(LoadReportTest)

#include "tst_load_report.moc"
