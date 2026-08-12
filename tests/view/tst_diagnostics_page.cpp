#include <QtTest/QtTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTreeWidget>

#include "application/ImportService.h"
#include "application/LibraryOrganizer.h"
#include "application/SizeService.h"
#include "support/PathText.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeLoadingReportSource.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSceneryCache.h"
#include "tests/doubles/FakeSceneryParser.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "view/delegates/RowDelegate.h"
#include "view/diagnostics/DiagnosticsPage.h"
#include "view/diagnostics/LoadPanel.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class DiagnosticsPageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void BuildingAndTearingDownAloneDoesNotCrash();
        static void WhatSupportsTheNameIsQuietInTheThreeTables();
        static void TheThreeTablesKeepTheShippedRowHeight();
        static void WithNoReportTheLoadSectionSaysSoAndTheRestOfTheScreenStillAnswers();
        static void TheLoadSectionNamesTheAddonBehindAPackageAndTheReportsNameForTheRest();
        static void NoWordOnTheLoadSectionOffersATimePerAddon();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    TreeNode LibraryTree()
    {
        TreeNode aircrafts;
        aircrafts.kind = TreeNodeKind::Category;
        aircrafts.path = "D:/MSFS 2024/Aircrafts";
        aircrafts.children = {AddonNode("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w")};

        TreeNode node;
        node.kind = TreeNodeKind::Library;
        node.path = kLibrary;
        node.children = {aircrafts};

        return node;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
            fileSystem.AddFile("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/model.bin", 4096);
            fileSystem.AddLink("E:/Flight Simulator 2024/Community/gone", "D:/Removed/gone");
            catalog.SetTree(kLibrary, LibraryTree());

            session.ShowActiveProfile();
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        StartupOverFakes startup{filesystemProbe};

        ProfileService service{catalog, filesystemProbe, sidecars,        classifier,        linking,
                               log,     identities,      startup.service, LinkType::Junction};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeProcessProbe processProbe;
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        ImportEngine importEngine{filesystemProbe, files, sidecars, linking, log, LinkType::Junction};
        ImportService imports{importEngine, processProbe, filesystemProbe,   catalog, files, sidecars,
                              linking,      log,          LinkType::Junction};
        FakeSettingsRepository settings{SettingsWith(Profile())};
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, settings.stored, processProbe, runner, notifier};
        SizeService sizes{catalog, filesystemProbe, clock, runner};
        FakeSceneryParser sceneryParser;
        FakeSceneryCache sceneryCache;
        SceneryService scenery{filesystemProbe, sceneryParser, clock, sceneryCache};
        FakeLoadingReportSource loading;
        DiagnosticsViewModel viewModel{imports, sizes, scenery, session, loading, clock, runner};
    };

    QTreeWidget* TableNamed(const DiagnosticsPage& page, const QString& name)
    {
        return page.findChild<QTreeWidget*>(name);
    }

    QTreeWidgetItem* DeepestOf(QTreeWidgetItem* item)
    {
        return item->childCount() == 0 ? item : DeepestOf(item->child(0));
    }
}

void DiagnosticsPageTest::BuildingAndTearingDownAloneDoesNotCrash()
{
    Fixture f;
    {
        DiagnosticsPage page(f.viewModel);
    }
}

void DiagnosticsPageTest::WhatSupportsTheNameIsQuietInTheThreeTables()
{
    Fixture f;
    DiagnosticsPage page(f.viewModel);

    f.viewModel.Show();
    f.viewModel.ShowSize();

    const QTreeWidget* counts = TableNamed(page, QStringLiteral("DiagnosticsCounts"));
    const QTreeWidget* troubled = TableNamed(page, QStringLiteral("DiagnosticsTroubled"));
    const QTreeWidget* sizes = TableNamed(page, QStringLiteral("DiagnosticsSizes"));

    QVERIFY(counts != nullptr);
    QVERIFY(troubled != nullptr);
    QVERIFY(sizes != nullptr);

    QVERIFY(counts->topLevelItemCount() > 0);
    QVERIFY(!counts->topLevelItem(0)->data(0, QuietRole).toBool());
    QVERIFY(counts->topLevelItem(0)->data(1, QuietRole).toBool());

    QTreeWidgetItem* broken = DeepestOf(troubled->topLevelItem(0));
    QVERIFY(broken->parent() != nullptr);
    QVERIFY(!broken->data(0, QuietRole).toBool());
    QVERIFY(broken->data(1, QuietRole).toBool());

    QVERIFY(sizes->topLevelItemCount() > 0);
    QTreeWidgetItem* addon = DeepestOf(sizes->topLevelItem(0));
    QVERIFY(!addon->data(0, QuietRole).toBool());
    QVERIFY(addon->data(1, QuietRole).toBool());
    QVERIFY(addon->data(2, QuietRole).toBool());
}

void DiagnosticsPageTest::TheThreeTablesKeepTheShippedRowHeight()
{
    Fixture f;
    DiagnosticsPage page(f.viewModel);
    page.resize(1200, 700);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));

    f.viewModel.Show();
    f.viewModel.ShowSize();

    const RowDelegate asShipped;
    RowDelegate shortened;
    shortened.KeepRowsAtLeast(0);

    QStyleOptionViewItem item;
    item.font = page.font();
    item.fontMetrics = QFontMetrics(item.font);

    for (const QString& name : {QStringLiteral("DiagnosticsCounts"), QStringLiteral("DiagnosticsTroubled"),
                                QStringLiteral("DiagnosticsSizes")})
    {
        QTreeWidget* table = TableNamed(page, name);
        QVERIFY(table != nullptr);

        const QModelIndex first = table->model()->index(0, 0, {});
        QVERIFY(first.isValid());

        const int shipped = asShipped.sizeHint(item, first).height();
        QVERIFY(shipped > shortened.sizeHint(item, first).height());
        QCOMPARE(table->visualItemRect(table->topLevelItem(0)).height(), shipped);
    }
}

void DiagnosticsPageTest::WithNoReportTheLoadSectionSaysSoAndTheRestOfTheScreenStillAnswers()
{
    Fixture f;
    DiagnosticsPage page(f.viewModel);

    f.viewModel.Show();
    f.viewModel.ShowTheLoad();

    const QTreeWidget* modules = TableNamed(page, QStringLiteral("DiagnosticsModules"));

    QVERIFY(modules != nullptr);
    QCOMPARE(modules->topLevelItemCount(), 0);
    QVERIFY(!modules->isVisible());
    QVERIFY(!f.viewModel.Load().reportWasRead);
    QVERIFY(!f.viewModel.Counts().empty());
}

void DiagnosticsPageTest::TheLoadSectionNamesTheAddonBehindAPackageAndTheReportsNameForTheRest()
{
    Fixture f;
    f.loading.ReportAModule("fmc.wasm", "pmdg-aircraft-77w", 327680);
    f.loading.ReportAModule("fsdt-msfs-bridge.wasm", "fsdreamteam-gsx-pro", 268763136);
    f.loading.ReportPackagesRegistered(264);

    DiagnosticsPage page(f.viewModel);

    f.viewModel.Show();
    f.viewModel.ShowTheLoad();

    const QTreeWidget* modules = TableNamed(page, QStringLiteral("DiagnosticsModules"));

    QCOMPARE(modules->topLevelItemCount(), 2);
    QCOMPARE(modules->topLevelItem(0)->text(0), QStringLiteral("fsdt-msfs-bridge.wasm"));
    QCOMPARE(modules->topLevelItem(1)->text(0), QStringLiteral("fmc.wasm"));
    QCOMPARE(modules->topLevelItem(1)->text(2), AsText(std::filesystem::path("Aircrafts") / "pmdg-aircraft-77w"));
    QVERIFY(!modules->topLevelItem(1)->data(2, QuietRole).toBool());
    QCOMPARE(modules->topLevelItem(0)->text(1), QStringLiteral("fsdreamteam-gsx-pro"));
    QVERIFY(modules->topLevelItem(0)->data(2, QuietRole).toBool());
}

void DiagnosticsPageTest::NoWordOnTheLoadSectionOffersATimePerAddon()
{
    Fixture f;
    f.loading.ReportAModule("fmc.wasm", "pmdg-aircraft-77w", 327680);
    f.loading.ReportPackagesRegistered(264);

    DiagnosticsPage page(f.viewModel);

    f.viewModel.Show();
    f.viewModel.ShowTheLoad();

    const LoadPanel* panel = page.findChild<LoadPanel*>();

    QVERIFY(panel != nullptr);

    for (const QLabel* said : panel->findChildren<QLabel*>())
    {
        QVERIFY(!said->text().contains(QStringLiteral("second"), Qt::CaseInsensitive));
        QVERIFY(!said->text().contains(QStringLiteral("minute"), Qt::CaseInsensitive));
    }

    const QTreeWidget* modules = TableNamed(page, QStringLiteral("DiagnosticsModules"));

    for (int column = 0; column < modules->columnCount(); ++column)
    {
        QVERIFY(!modules->headerItem()->text(column).contains(QStringLiteral("time"), Qt::CaseInsensitive));
    }
}

QTEST_MAIN(DiagnosticsPageTest)

#include "tst_diagnostics_page.moc"
