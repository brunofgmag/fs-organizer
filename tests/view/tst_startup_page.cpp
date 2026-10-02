#include <QtCore/QTimer>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QtTest>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStyleOptionViewItem>
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
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/FakeStartupEntries.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/ButtonLookup.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "view/delegates/RowDelegate.h"
#include "view/simulator/StartupPage.h"
#include "view/theme/ModernistTheme.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/SessionNotifier.h"
#include "tests/support/PageFloor.h"

namespace
{
    class StartupPageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ThePageFitsTheNarrowestWindow();
        static void TheCheckOpensWithTheSameInsetAsTheTextOfACellWithoutOne();
        static void AClickOnTheDrawnCheckTogglesTheEntry_data();
        static void AClickOnTheDrawnCheckTogglesTheEntry();
        static void AClickWhereTheCheckUsedToBeLeftOfTheDrawnOneDoesNothing();
        static void ATableThatDoesNotAskForItKeepsTheCheckWhereTheStyleLaysIt();
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

    AppSettings Stored(const bool managing)
    {
        AppSettings settings = SettingsWith(Active());
        settings.manageStartupEntries = managing;

        return settings;
    }

    struct Fixture
    {
        explicit Fixture(const bool managing = true) : settings(Stored(managing))
        {
            fileSystem.AddDirectory(kDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory("D:/Library/Utilities");
            fileSystem.AddDirectory(kFlowInTheLibrary);
            fileSystem.AddFile(kFlowInTheLibrary / "manifest.json");
            fileSystem.AddFile(kFlowExecutable);
            fileSystem.AddFile(kSimlink);

            catalog.SetTree(kLibrary, LibraryTree());

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
        StartupOverFakes startup{filesystemProbe};

        ProfileService profiles{catalog, filesystemProbe, sidecars,        classifier,        linking,
                                log,     identities,      startup.service, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier{};
        Session session{profiles, organizer, settings, settings.stored, processProbe, runner, notifier};
        FakeStartupEntries entries;
        StartupService service{entries, processProbe, filesystemProbe, true};
        StartupViewModel viewModel{service, session, clock};
        StartupPage page{viewModel};
    };

    QTreeWidget* TableOf(const StartupPage& page)
    {
        return page.findChild<QTreeWidget*>();
    }

    constexpr int kCellWidth = 200;
    constexpr int kCellHeight = 29;

    QTreeWidget* ShownTable(Fixture& fixture)
    {
        fixture.page.resize(1024, 600);
        fixture.page.show();
        static_cast<void>(QTest::qWaitForWindowExposed(&fixture.page));
        QCoreApplication::processEvents();

        return TableOf(fixture.page);
    }

    QStyleOptionViewItem OptionFor(const QTreeWidget& table, const QRect& rect)
    {
        QStyleOptionViewItem option;
        option.initFrom(table.viewport());
        option.widget = &table;
        option.rect = rect;
        option.state |= QStyle::State_Enabled | QStyle::State_Active;
        option.features |= QStyleOptionViewItem::HasDisplay;

        return option;
    }

    QRect InkOf(const QImage& image)
    {
        const QRgb ground = image.pixel(0, 0);
        QRect ink;

        for (int y = 0; y < image.height(); ++y)
        {
            for (int x = 0; x < image.width(); ++x)
            {
                if (image.pixel(x, y) != ground)
                {
                    ink = ink.isNull() ? QRect(x, y, 1, 1) : ink.united(QRect(x, y, 1, 1));
                }
            }
        }

        return ink;
    }

    QImage BlankFor(const QSize& size)
    {
        QImage image(size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);

        return image;
    }

    QImage PaintedCell(const QTreeWidget& table, const int row, const int column)
    {
        QImage image = BlankFor(QSize(kCellWidth, kCellHeight));
        QPainter painter(&image);

        table.itemDelegate()->paint(&painter, OptionFor(table, QRect(0, 0, kCellWidth, kCellHeight)),
                                    table.model()->index(row, column));

        return image;
    }

    QRect InkOfTheCheckAlone(const QTreeWidget& table, const QStyle::State state)
    {
        constexpr int kMargin = 8;

        QStyleOptionViewItem laid = OptionFor(table, QRect(0, 0, kCellWidth, kCellHeight));
        laid.features |= QStyleOptionViewItem::HasCheckIndicator;

        const QSize box = table.style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &laid, &table).size();

        QImage image = BlankFor(box + QSize(2 * kMargin, 2 * kMargin));
        QPainter painter(&image);

        QStyleOptionViewItem check = OptionFor(table, QRect(QPoint(kMargin, kMargin), box));
        check.state |= state;
        table.style()->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &check, &painter, &table);
        painter.end();

        return InkOf(image).translated(-kMargin, -kMargin);
    }

    QImage TheTextAlone(const QTreeWidget& table, const QString& text)
    {
        QImage image = BlankFor(QSize(kCellWidth, kCellHeight));
        QPainter painter(&image);

        painter.setFont(table.viewport()->font());
        painter.drawText(QRect(0, 0, kCellWidth, kCellHeight), Qt::AlignLeft | Qt::AlignVCenter, text);

        return image;
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
    QVERIFY(ButtonSaying(fixture.page, "Manage startup entries") != nullptr);
    QCOMPARE(fixture.entries.reads, std::size_t{0});
}

void StartupPageTest::TurningItOnFromTheLooseStateShowsTheEntries()
{
    Fixture fixture(false);
    fixture.viewModel.Show();

    ButtonSaying(fixture.page, "Manage startup entries")->click();

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

    QVERIFY(ButtonSaying(fixture.page, "Manage startup entries") != nullptr);
    QCOMPARE(fixture.page.findChild<QStackedWidget*>()->currentIndex(), 2);
    QCOMPARE(fixture.entries.reads, std::size_t{0});
}

void StartupPageTest::TheCheckOpensWithTheSameInsetAsTheTextOfACellWithoutOne()
{
    ApplyModernistTheme(*qApp);
    Fixture fixture;
    fixture.viewModel.Show();
    QTreeWidget* table = ShownTable(fixture);

    const QRect checkOnItsOwn = InkOfTheCheckAlone(*table, QStyle::State_On);
    const QImage checkedCell = PaintedCell(*table, 0, 0);
    const int checkInset = InkOf(checkedCell).left() - checkOnItsOwn.left();

    const QString path = table->model()->index(0, 1).data(Qt::DisplayRole).toString();
    const int textInset = InkOf(PaintedCell(*table, 0, 1)).left() - InkOf(TheTextAlone(*table, path)).left();

    QVERIFY(textInset > 0);
    QCOMPARE(checkInset, textInset);
}

void StartupPageTest::AClickOnTheDrawnCheckTogglesTheEntry_data()
{
    QTest::addColumn<bool>("atTheFarEdge");

    QTest::newRow("at the centre") << false;
    QTest::newRow("at the far edge") << true;
}

void StartupPageTest::AClickOnTheDrawnCheckTogglesTheEntry()
{
    QFETCH(bool, atTheFarEdge);

    ApplyModernistTheme(*qApp);
    Fixture fixture;
    fixture.viewModel.Show();
    QTreeWidget* table = ShownTable(fixture);

    const QRect cell = table->visualRect(table->model()->index(1, 0));
    const QRect drawn = InkOfTheCheckAlone(*table, QStyle::State_Off);
    const int drawnLeft = InkOf(PaintedCell(*table, 1, 0)).left();
    const int inside = atTheFarEdge ? drawn.width() - 2 : drawn.width() / 2;

    QCOMPARE(table->topLevelItem(1)->checkState(0), Qt::Unchecked);

    QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(cell.left() + drawnLeft + inside, cell.center().y()));

    QCOMPARE(fixture.entries.writes, std::size_t{1});
    QCOMPARE(table->topLevelItem(1)->checkState(0), Qt::Checked);
}

void StartupPageTest::AClickWhereTheCheckUsedToBeLeftOfTheDrawnOneDoesNothing()
{
    ApplyModernistTheme(*qApp);
    Fixture fixture;
    fixture.viewModel.Show();
    QTreeWidget* table = ShownTable(fixture);

    const QRect cell = table->visualRect(table->model()->index(1, 0));
    const int drawnLeft = InkOf(PaintedCell(*table, 1, 0)).left();

    QVERIFY(drawnLeft > 2);

    QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(cell.left() + drawnLeft - 2, cell.center().y()));

    QCOMPARE(fixture.entries.writes, std::size_t{0});
    QCOMPARE(table->topLevelItem(1)->checkState(0), Qt::Unchecked);
}

void StartupPageTest::ATableThatDoesNotAskForItKeepsTheCheckWhereTheStyleLaysIt()
{
    ApplyModernistTheme(*qApp);

    QTreeWidget tree;
    tree.setColumnCount(1);
    tree.setItemDelegate(new RowDelegate(&tree));

    auto* item = new QTreeWidgetItem(&tree);
    item->setCheckState(0, Qt::Checked);

    tree.resize(400, 200);
    tree.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tree));

    QStyleOptionViewItem option = OptionFor(tree, QRect(0, 0, kCellWidth, kCellHeight));
    option.features |= QStyleOptionViewItem::HasCheckIndicator;
    const int laidByTheStyle =
        tree.style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &option, &tree).left();

    const int drawnAt = InkOf(PaintedCell(tree, 0, 0)).left() - InkOfTheCheckAlone(tree, QStyle::State_On).left();

    QVERIFY(laidByTheStyle > 0);
    QCOMPARE(drawnAt, laidByTheStyle);
}

void StartupPageTest::ThePageFitsTheNarrowestWindow()
{
    Fixture f;

    ItFitsTheNarrowestWindow(f.page, "The startup half of the simulator page");
}

QTEST_MAIN(StartupPageTest)

#include "tst_startup_page.moc"
