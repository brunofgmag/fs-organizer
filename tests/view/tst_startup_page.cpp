#include <QtCore/QTimer>
#include <QtTest/QtTest>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTreeWidget>

#include <cstddef>
#include <filesystem>
#include <vector>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/FakeStartupEntries.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "view/simulator/StartupPage.h"
#include "view/theme/ModernistTheme.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class StartupPageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void BuildingAndTearingDownAloneDoesNotCrash();
        static void EachEntryLandsOnItsOwnRowWithTheSwitchItCarriesOnDisk();
        static void TheRowOfAnAlarmingEntryIsMarkedAlarmingInEveryColumn();
        static void AnEntryInsideAnAddonWithNothingWrongCarriesATagAndNotAnAlarm();
        static void WhatSupportsTheNameIsQuietAndWhatAlarmsKeepsTheNameInk();
        static void TickingARowWritesTheSwitchAndTheRowStaysWhereTheDiskPutIt();
        static void WithTheSimulatorOpenTheRowGoesBackToWhatTheDiskSays();
        static void TheLooseStateOffersToTurnItOnInsteadOfShowingAnEmptyTable();
        static void TurningItOnFromTheLooseStateShowsTheEntries();
        static void ALanguageChangeReachesTheToolbarAndTheLooseState();
    };

    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kFlowInTheLibrary = "D:/Library/Utilities/p42-util-flow-pro";
    const std::filesystem::path kFlowExecutable = "E:/Sim/Community/p42-util-flow-pro/bin/flow.exe";
    const std::filesystem::path kSimlink = "C:/Program Files/Navigraph/Simlink/simlink.exe";
    const std::filesystem::path kGone = "C:/Program Files/Ghost/ghost.exe";

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
        TreeNode utilities;
        utilities.kind = TreeNodeKind::Category;
        utilities.path = "D:/Library/Utilities";
        utilities.children = {AddonNode(kFlowInTheLibrary)};

        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = {std::move(utilities)};

        return library;
    }

    SimulatorProfile Active()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kDestination};
        profile.defaultDestination = kDestination;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    struct Fixture
    {
        explicit Fixture(const bool managing = true)
        {
            fileSystem.AddDirectory(kDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory("D:/Library/Utilities");
            fileSystem.AddDirectory(kFlowInTheLibrary);
            fileSystem.AddFile(kFlowInTheLibrary / "manifest.json");
            fileSystem.AddFile(kFlowExecutable);
            fileSystem.AddFile(kSimlink);

            catalog.SetTree(kLibrary, LibraryTree());

            settings.stored.profiles = {Active()};
            settings.stored.activeProfileId = Active().id;
            settings.stored.manageStartupEntries = managing;

            entries.Carry(StartupEntry{.label = "FlowPro", .path = kFlowExecutable, .enabled = true});
            entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink, .enabled = false});
            entries.Carry(StartupEntry{.label = "Ghost", .path = kGone, .enabled = true});

            service.Manage(managing);
            session.ShowActiveProfile();
        }

        InMemoryFileSystem fileSystem;
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeLinkService linkService{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeProcessProbe processProbe;
        FakeLibraryIdGenerator identities;
        EntryClassifier classifier{linkService, filesystemProbe};
        LinkingEngine linking{linkService, filesystemProbe};
        ProfileService profiles{catalog, filesystemProbe, sidecars,          classifier, linking,
                                log,     identities,      LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier{};
        Session session{profiles, organizer, settings, processProbe, runner, notifier};
        FakeStartupEntries entries;
        StartupService service{entries, processProbe, filesystemProbe, true};
        StartupViewModel viewModel{service, session, settings, clock};
        StartupPage page{viewModel};
    };

    QTreeWidget* TableOf(const StartupPage& page)
    {
        return page.findChild<QTreeWidget*>();
    }

    QPushButton* ButtonSaying(const StartupPage& page, const QString& text)
    {
        for (QPushButton* button : page.findChildren<QPushButton*>())
        {
            if (button->text() == text)
            {
                return button;
            }
        }

        return nullptr;
    }
}

void StartupPageTest::BuildingAndTearingDownAloneDoesNotCrash()
{
    const Fixture fixture;

    QVERIFY(TableOf(fixture.page) != nullptr);
}

void StartupPageTest::EachEntryLandsOnItsOwnRowWithTheSwitchItCarriesOnDisk()
{
    Fixture fixture;
    fixture.viewModel.Show();

    const QTreeWidget* table = TableOf(fixture.page);

    QCOMPARE(table->topLevelItemCount(), 3);
    QCOMPARE(table->topLevelItem(0)->text(0), QString("FlowPro"));
    QCOMPARE(table->topLevelItem(0)->checkState(0), Qt::Checked);
    QCOMPARE(table->topLevelItem(1)->checkState(0), Qt::Unchecked);
    QCOMPARE(table->topLevelItem(2)->text(0), QString("Ghost"));
}

void StartupPageTest::TheRowOfAnAlarmingEntryIsMarkedAlarmingInEveryColumn()
{
    Fixture fixture;
    fixture.viewModel.Show();

    const QTreeWidget* table = TableOf(fixture.page);
    const QTreeWidgetItem* ghost = table->topLevelItem(2);

    for (int column = 0; column < table->columnCount(); ++column)
    {
        QVERIFY(ghost->data(column, AlarmingRole).toBool());
    }

    QVERIFY(!table->topLevelItem(0)->data(0, AlarmingRole).toBool());
    QVERIFY(!ghost->text(2).isEmpty());
}

void StartupPageTest::AnEntryInsideAnAddonWithNothingWrongCarriesATagAndNotAnAlarm()
{
    Fixture fixture;
    fixture.viewModel.Show();

    const QTreeWidgetItem* flow = TableOf(fixture.page)->topLevelItem(0);

    QVERIFY(!flow->data(2, TagTextRole).toString().isEmpty());
    QVERIFY(flow->text(2).isEmpty());
    QVERIFY(!flow->data(2, AlarmingRole).toBool());
}

void StartupPageTest::WhatSupportsTheNameIsQuietAndWhatAlarmsKeepsTheNameInk()
{
    Fixture fixture;
    fixture.viewModel.Show();

    const QTreeWidget* table = TableOf(fixture.page);
    const QTreeWidgetItem* outside = table->topLevelItem(1);
    const QTreeWidgetItem* ghost = table->topLevelItem(2);

    QVERIFY(!outside->data(0, QuietRole).toBool());
    QVERIFY(outside->data(1, QuietRole).toBool());
    QVERIFY(outside->data(2, QuietRole).toBool());

    QVERIFY(ghost->data(1, QuietRole).toBool());
    QVERIFY(!ghost->data(2, QuietRole).toBool());
}

void StartupPageTest::TickingARowWritesTheSwitchAndTheRowStaysWhereTheDiskPutIt()
{
    Fixture fixture;
    fixture.viewModel.Show();

    QTreeWidget* table = TableOf(fixture.page);
    table->topLevelItem(1)->setCheckState(0, Qt::Checked);

    QCOMPARE(fixture.entries.writes, std::size_t{1});
    QCOMPARE(TableOf(fixture.page)->topLevelItem(1)->checkState(0), Qt::Checked);
    QVERIFY(fixture.viewModel.Lines()[1].enabled);
}

void StartupPageTest::WithTheSimulatorOpenTheRowGoesBackToWhatTheDiskSays()
{
    Fixture fixture;
    fixture.viewModel.Show();
    fixture.processProbe.ReportTheSimulatorAsRunning();

    QTimer::singleShot(0, &fixture.page,
                       []
                       {
                           if (QWidget* blocked = QApplication::activeModalWidget())
                           {
                               blocked->close();
                           }
                       });

    QTreeWidget* table = TableOf(fixture.page);
    table->topLevelItem(0)->setCheckState(0, Qt::Unchecked);

    QCOMPARE(fixture.entries.writes, std::size_t{0});
    QCOMPARE(TableOf(fixture.page)->topLevelItem(0)->checkState(0), Qt::Checked);
}

void StartupPageTest::TheLooseStateOffersToTurnItOnInsteadOfShowingAnEmptyTable()
{
    Fixture fixture(false);
    fixture.viewModel.Show();

    const QStackedWidget* panes = fixture.page.findChild<QStackedWidget*>();

    QCOMPARE(panes->currentIndex(), 2);
    QVERIFY(ButtonSaying(fixture.page, "Manage these") != nullptr);
    QCOMPARE(fixture.entries.reads, std::size_t{0});
}

void StartupPageTest::TurningItOnFromTheLooseStateShowsTheEntries()
{
    Fixture fixture(false);
    fixture.viewModel.Show();

    ButtonSaying(fixture.page, "Manage these")->click();

    QCOMPARE(fixture.page.findChild<QStackedWidget*>()->currentIndex(), 0);
    QCOMPARE(TableOf(fixture.page)->topLevelItemCount(), 3);
    QVERIFY(fixture.settings.stored.manageStartupEntries);
}

void StartupPageTest::ALanguageChangeReachesTheToolbarAndTheLooseState()
{
    Fixture fixture(false);
    fixture.viewModel.Show();

    QEvent language(QEvent::LanguageChange);
    QCoreApplication::sendEvent(&fixture.page, &language);

    QVERIFY(ButtonSaying(fixture.page, "Manage these") != nullptr);
    QCOMPARE(fixture.page.findChild<QStackedWidget*>()->currentIndex(), 2);
    QCOMPARE(fixture.entries.reads, std::size_t{0});
}

QTEST_MAIN(StartupPageTest)

#include "tst_startup_page.moc"
