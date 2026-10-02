#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QtTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStyleOptionViewItem>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeWidget>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/model/Preset.h"
#include "support/MomentText.h"
#include "support/PathText.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakePresetRepository.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/FakeStartupEntries.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/ButtonLookup.h"
#include "tests/support/CatalogueBesideTheBuild.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/ScrollBarCapRule.h"
#include "view/delegates/RowDelegate.h"
#include "view/simulator/StartupDraftDialog.h"
#include "view/simulator/StartupPage.h"
#include "view/simulator/UndoButton.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistTheme.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/TagTone.h"
#include "viewmodel/SessionNotifier.h"
#include "tests/support/PageFloor.h"

namespace
{
    class StartupPageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void cleanup();

        static void TheBarFitsTheNarrowestWindowInBothLanguagesAndBothViews_data();
        static void TheBarFitsTheNarrowestWindowInBothLanguagesAndBothViews();
        static void TheUndoButtonCutsTheNameToItsFloorAndKeepsTheWholeLabelInTheTip();
        static void TheCheckOpensWithTheSameInsetAsTheTextOfACellWithoutOne();
        static void AClickOnTheDrawnCheckTogglesTheEntry_data();
        static void AClickOnTheDrawnCheckTogglesTheEntry();
        static void AClickWhereTheCheckUsedToBeLeftOfTheDrawnOneDoesNothing();
        static void ATableThatDoesNotAskForItKeepsTheCheckWhereTheStyleLaysIt();
        static void BuildingAndTearingDownAloneDoesNotCrash();
        static void EachEntryLandsOnItsOwnRowWithTheSwitchItCarriesOnDisk();
        static void TheRowOfAnAlarmingEntryIsMarkedAlarmingInEveryColumn();
        static void AnEntryInsideAnAddonWithNothingWrongSaysSoInTextAndNeitherTagsNorAlarms();
        static void TheStateColumnSaysEachConditionAndOnlyABrokenProgramWearsTheChip_data();
        static void TheStateColumnSaysEachConditionAndOnlyABrokenProgramWearsTheChip();
        static void WhatSupportsTheNameIsQuietAndWhatAlarmsKeepsTheNameInk();
        static void TickingARowWritesTheSwitchAndTheRowStaysWhereTheDiskPutIt();
        static void WithTheSimulatorOpenTheRowGoesBackToWhatTheDiskSays();
        static void TheLooseStateOffersToTurnItOnInsteadOfShowingAnEmptyTable();
        static void TurningItOnFromTheLooseStateShowsTheEntries();
        static void ALanguageChangeReachesTheToolbarAndTheLooseState();

        static void TheMainBarOffersTheGesturesInOrderAndTheRemovedBarSwapsThem();
        static void WithoutARowEditRemoveRestoreAndDiscardWait();
        static void TheChipsCountTheFileAndTheRemovedAndSwitchTheView();
        static void TheRefreshButtonCarriesTheMomentOfTheReadingAndTheBarNoLongerSaysIt();
        static void OnlyOneRowIsSelectedAndAPathIsCutInTheMiddle();
        static void RemovingFromTheBarAsksAndTakesTheEntryOut();
        static void CancellingTheQuestionLeavesTheEntryWhereItWas();
        static void DeleteRemovesTheSelectedRowAfterTheQuestion();
        static void TheContextMenuOffersTheGesturesOfTheViewItIsIn();
        static void EditOpensFromTheBarTheKeyboardAndADoubleClickOutsideTheCheck_data();
        static void EditOpensFromTheBarTheKeyboardAndADoubleClickOutsideTheCheck();
        static void ADoubleClickOnTheCheckDoesNotOpenTheEdit();
        static void ADoubleClickOnTheCheckOfAnotherRowNeverOpensTheEditOfTheSelectedOne();
        static void AnEditThatChangesNothingSaysSoInsteadOfSaved();
        static void AMovedPathSaysHowManyPresetsFollowedTheReturnPresetIncluded();
        static void AMovedPathNamesTheReturnPresetThatCouldNotFollow();
        static void TheUndoButtonKeepsAnAmpersandOfTheNameAsALetter();
        static void TheScrollBarOfBothTablesIsCappedAndTheCapFollowsTheBar();
        static void EditingWritesWhatTheDialogSaysAndAPresetThatCouldNotFollowIsNamed();
        static void AddingOpensTheDialogFromTheBarAndFromTheEmptyState();
        static void UndoingWorksFromTheButtonAndFromControlZ();
        static void RestoringAndDiscardingWorkFromTheRemovedView();
        static void TheQuestionToRemoveFollowsTheConditionWithEachSentenceOnItsOwnLine_data();
        static void TheQuestionToRemoveFollowsTheConditionWithEachSentenceOnItsOwnLine();
        static void TheQuestionToDiscardSaysItIsForGood();
        static void TheEmptyStatesOfBothViewsSaySoAndTheMainOneOffersToAdd();
    };

    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kFlowInTheLibrary = "D:/Library/Utilities/p42-util-flow-pro";
    const std::filesystem::path kFlowExecutable = "E:/Sim/Community/p42-util-flow-pro/bin/flow.exe";
    const std::filesystem::path kSimlink = "C:/Program Files/Navigraph/Simlink/simlink.exe";
    const std::filesystem::path kGone = "C:/Program Files/Ghost/ghost.exe";
    const std::filesystem::path kAny2Gsx = "C:/Tools/Any2GSX/Any2GSX.exe";
    const std::filesystem::path kFlowTool = kFlowInTheLibrary / "bin" / "tool.exe";
    const std::filesystem::path kBehindTheAddon = "E:/Sim/Community/p42-util-flow-pro/bin/other.exe";
    const std::filesystem::path kOnAbsentDrive = "G:/Tools/vRAAS/vRAAS.exe";

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
        explicit Fixture(const bool managing = true, const bool withEntries = true) : settings(Stored(managing))
        {
            fileSystem.AddDirectory(kDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory("D:/Library/Utilities");
            fileSystem.AddDirectory(kFlowInTheLibrary);
            fileSystem.AddFile(kFlowInTheLibrary / "manifest.json");
            fileSystem.AddFile(kFlowExecutable);
            fileSystem.AddFile(kSimlink);
            fileSystem.AddFile(kAny2Gsx);
            fileSystem.AddFile(kFlowTool);

            catalog.SetTree(kLibrary, LibraryTree());

            if (withEntries)
            {
                entries.Carry(StartupEntry{.label = "FlowPro", .path = kFlowExecutable, .enabled = true});
                entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink, .enabled = false});
                entries.Carry(StartupEntry{.label = "Ghost", .path = kGone, .enabled = true});
            }

            service.Manage(managing);
            session.ShowActiveProfile();
            entries.reads = 0;
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
        FakeStartupEntries& entries = startup.entries;
        StartupService service{entries, processProbe, filesystemProbe, true};
        FakePresetRepository presets;
        StartupEditor editor{service, presets, filesystemProbe, log};
        StartupViewModel viewModel{service, editor, session, clock};
        StartupPage page{viewModel};
    };

    QTreeWidget* TableOf(const StartupPage& page)
    {
        return page.findChild<QTreeWidget*>(QStringLiteral("StartupEntries"));
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

    constexpr int kTries = 200;
    constexpr int kTryEvery = 10;

    int& Generation()
    {
        static int generation = 0;

        return generation;
    }

    const QString kLongestName = QStringLiteral("FS2Crew Fenix A320 Custom Liveries Patcher");

    struct TheBox
    {
        bool shown = false;
        QString title{};
        QString asked{};
        QString explained{};
        int askedMinimumWidth = 0;
        QStringList buttons{};
    };

    void
    AnswerTheNextBox(TheBox& seen, const QString& clicking, const int tries = kTries, const int armedAt = Generation())
    {
        QTimer::singleShot(kTryEvery, qApp,
                           [&seen, clicking, tries, armedAt]
                           {
                               if (armedAt != Generation())
                               {
                                   return;
                               }

                               auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());

                               if (box == nullptr)
                               {
                                   if (tries > 0)
                                   {
                                       AnswerTheNextBox(seen, clicking, tries - 1, armedAt);
                                   }

                                   return;
                               }

                               seen.shown = true;
                               seen.title = box->windowTitle();
                               seen.asked = box->text();
                               seen.explained = box->informativeText();

                               if (const auto* label = box->findChild<QLabel*>(QStringLiteral("qt_msgbox_label")))
                               {
                                   seen.askedMinimumWidth = label->minimumWidth();
                               }

                               for (QAbstractButton* button : box->buttons())
                               {
                                   seen.buttons << button->text();
                               }

                               for (QAbstractButton* button : box->buttons())
                               {
                                   if (button->text() == clicking)
                                   {
                                       button->click();
                                   }
                               }
                           });
    }

    void FillTheNextDialog(bool& opened,
                           const std::function<void(StartupDraftDialog&)>& fill,
                           const int tries = kTries,
                           const int armedAt = Generation())
    {
        QTimer::singleShot(kTryEvery, qApp,
                           [&opened, fill, tries, armedAt]
                           {
                               if (armedAt != Generation())
                               {
                                   return;
                               }

                               auto* dialog = qobject_cast<StartupDraftDialog*>(QApplication::activeModalWidget());

                               if (dialog == nullptr)
                               {
                                   if (tries > 0)
                                   {
                                       FillTheNextDialog(opened, fill, tries - 1, armedAt);
                                   }

                                   return;
                               }

                               opened = true;
                               fill(*dialog);
                               if (dialog->isVisible())
                               {
                                   dialog->reject();
                               }
                           });
    }

    void AcceptWith(StartupDraftDialog& dialog, const QString& button)
    {
        ButtonSaying(dialog, button)->click();
    }

    void ChooseFromTheNextMenu(QStringList& offered,
                               const QString& choose,
                               const int tries = kTries,
                               const int armedAt = Generation())
    {
        QTimer::singleShot(kTryEvery, qApp,
                           [&offered, choose, tries, armedAt]
                           {
                               if (armedAt != Generation())
                               {
                                   return;
                               }

                               auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());

                               if (menu == nullptr)
                               {
                                   if (tries > 0)
                                   {
                                       ChooseFromTheNextMenu(offered, choose, tries - 1, armedAt);
                                   }

                                   return;
                               }

                               for (QAction* action : menu->actions())
                               {
                                   offered << action->text();

                                   if (action->text() == choose)
                                   {
                                       action->trigger();
                                   }
                               }

                               menu->close();
                           });
    }

    constexpr int kInTheFile = 0;
    constexpr int kRemoved = 1;

    QToolButton* TheChip(const StartupPage& page, const int which)
    {
        return page.findChildren<QToolButton*>(QStringLiteral("FilterChip")).value(which);
    }

    QStringList TheButtonsOfTheBar(const StartupPage& page)
    {
        const auto* bar = page.findChild<QWidget*>(QStringLiteral("PageToolbar"));
        QStringList shown;

        for (const QPushButton* button : bar->findChildren<QPushButton*>(Qt::FindDirectChildrenOnly))
        {
            if (!button->isHidden())
            {
                shown << button->text();
            }
        }

        return shown;
    }

    QTreeWidget* RemovedOf(const StartupPage& page)
    {
        return page.findChild<QTreeWidget*>(QStringLiteral("RemovedEntries"));
    }

    int CurrentPane(const StartupPage& page)
    {
        return page.findChild<QStackedWidget*>()->currentIndex();
    }

    void Select(QTreeWidget* table, const int row)
    {
        table->setCurrentItem(table->topLevelItem(row));
        table->topLevelItem(row)->setSelected(true);
    }

    QStringList LabelsOf(const StartupViewModel& viewModel)
    {
        QStringList labels;

        for (const StartupLine& line : viewModel.Lines())
        {
            labels << QString::fromStdString(line.label);
        }

        return labels;
    }

    void FocusTheTable(StartupPage& page, QTreeWidget* table)
    {
        page.resize(kWidestAPageMayBe, 600);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        page.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&page));
        table->setFocus();
        QCoreApplication::processEvents();
    }

    class Installed
    {
    public:
        explicit Installed(const QString& language)
        {
            if (language == QLatin1String("en"))
            {
                return;
            }

            const QString file = TheCatalogueBesideTheBuild(language);

            found_ = !file.isEmpty() && catalogue_.load(file);
            installed_ = found_ && QCoreApplication::installTranslator(&catalogue_);
        }

        ~Installed()
        {
            if (installed_)
            {
                QCoreApplication::removeTranslator(&catalogue_);
            }
        }

        Installed(const Installed&) = delete;
        Installed& operator=(const Installed&) = delete;

        [[nodiscard]] bool Ready(const QString& language) const
        {
            return language == QLatin1String("en") || installed_;
        }

    private:
        QTranslator catalogue_;
        bool found_ = false;
        bool installed_ = false;
    };

    void CarryManyEntriesAndManyRemovedOnes(Fixture& fixture)
    {
        for (int each = 0; each < 120; ++each)
        {
            fixture.entries.Carry(StartupEntry{.label = "Program " + std::to_string(each),
                                               .path = "C:/Many/p" + std::to_string(each) + ".exe"});
            fixture.entries.CarryRemoved(StartupEntry{.label = "Gone " + std::to_string(each),
                                                      .path = "C:/Gone/g" + std::to_string(each) + ".exe"},
                                         {}, fixture.clock.now + std::chrono::seconds(each));
        }
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
}

void StartupPageTest::AnEntryInsideAnAddonWithNothingWrongSaysSoInTextAndNeitherTagsNorAlarms()
{
    Fixture fixture;
    fixture.viewModel.Show();

    const QTreeWidgetItem* flow = TableOf(fixture.page)->topLevelItem(0);

    QCOMPARE(flow->text(2), QString("inside an addon"));
    QVERIFY(flow->data(2, TagTextRole).toString().isEmpty());
    QVERIFY(!flow->data(2, AlarmingRole).toBool());
    QVERIFY(flow->data(2, QuietRole).toBool());
}

void StartupPageTest::TheStateColumnSaysEachConditionAndOnlyABrokenProgramWearsTheChip_data()
{
    QTest::addColumn<QString>("label");
    QTest::addColumn<bool>("enabled");
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("chip");
    QTest::addColumn<bool>("alarming");

    QTest::newRow("reachable outside") << "Navigraph Simlink" << false << "outside your addons" << "" << false;
    QTest::newRow("broken and enabled") << "Ghost" << true << "" << "the program is missing" << true;
    QTest::newRow("broken and disabled") << "Ghost off" << false << "" << "the program is missing" << false;
    QTest::newRow("behind a disabled addon, enabled") << "Behind" << true << "its addon is disabled" << "" << true;
    QTest::newRow("behind a disabled addon, disabled")
        << "Behind off" << false << "its addon is disabled" << "" << false;
    QTest::newRow("on a drive that is gone, enabled") << "Away" << true << "its drive is not connected" << "" << true;
    QTest::newRow("on a drive that is gone, disabled")
        << "Away off" << false << "its drive is not connected" << "" << false;
}

void StartupPageTest::TheStateColumnSaysEachConditionAndOnlyABrokenProgramWearsTheChip()
{
    QFETCH(const QString, label);
    QFETCH(const bool, enabled);
    QFETCH(const QString, text);
    QFETCH(const QString, chip);
    QFETCH(const bool, alarming);

    Fixture fixture;
    fixture.fileSystem.MarkVolumeUnavailable(kOnAbsentDrive);
    fixture.entries.Carry(
        StartupEntry{.label = "Ghost off", .path = "C:/Program Files/Ghost/ghost-off.exe", .enabled = false});
    fixture.entries.Carry(StartupEntry{.label = "Behind", .path = kBehindTheAddon, .enabled = true});
    fixture.entries.Carry(
        StartupEntry{.label = "Behind off", .path = kBehindTheAddon.parent_path() / "off.exe", .enabled = false});
    fixture.entries.Carry(StartupEntry{.label = "Away", .path = kOnAbsentDrive, .enabled = true});
    fixture.entries.Carry(StartupEntry{.label = "Away off", .path = "G:/Tools/vRAAS/off.exe", .enabled = false});
    fixture.viewModel.Show();

    const QTreeWidget* table = TableOf(fixture.page);
    const QTreeWidgetItem* row = nullptr;

    for (int at = 0; at < table->topLevelItemCount(); ++at)
    {
        if (table->topLevelItem(at)->text(0) == label)
        {
            row = table->topLevelItem(at);
        }
    }

    QVERIFY(row != nullptr);
    QCOMPARE(row->checkState(0) == Qt::Checked, enabled);
    QCOMPARE(row->text(2), text);
    QCOMPARE(row->data(2, TagTextRole).toString(), chip);
    QCOMPARE(row->data(2, TagToneRole).toInt(), static_cast<int>(TagTone::Outlined));

    for (int column = 0; column < table->columnCount(); ++column)
    {
        QCOMPARE(row->data(column, AlarmingRole).toBool(), alarming);
    }

    QCOMPARE(row->data(2, QuietRole).toBool(), !alarming);
    QVERIFY(row->data(1, QuietRole).toBool());
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

void StartupPageTest::cleanup()
{
    ++Generation();
}

void StartupPageTest::TheBarFitsTheNarrowestWindowInBothLanguagesAndBothViews_data()
{
    QTest::addColumn<QString>("language");
    QTest::addColumn<bool>("removed");

    QTest::newRow("English, in the file") << QStringLiteral("en") << false;
    QTest::newRow("English, removed") << QStringLiteral("en") << true;
    QTest::newRow("Portuguese, in the file") << QStringLiteral("pt_BR") << false;
    QTest::newRow("Portuguese, removed") << QStringLiteral("pt_BR") << true;
}

void StartupPageTest::TheBarFitsTheNarrowestWindowInBothLanguagesAndBothViews()
{
    QFETCH(const QString, language);
    QFETCH(const bool, removed);

    const Installed installed(language);
    QVERIFY2(installed.Ready(language), "app_pt_BR.qm is not beside the build: build the release_translations target");

    Fixture f;
    CarryManyEntriesAndManyRemovedOnes(f);
    f.viewModel.Show();
    QCOMPARE(f.viewModel.Add(StartupDraft{.label = kLongestName.toStdString(), .file = kAny2Gsx}).result,
             FileResult::Completed);

    if (removed)
    {
        TheChip(f.page, kRemoved)->click();
    }

    ItFitsTheNarrowestWindow(f.page, "The startup half of the simulator page");
    const QStringList buttons = TheButtonsOfTheBar(f.page);

    QCOMPARE(buttons.size(), removed ? 5 : 6);
    QVERIFY2(removed || ButtonSaying(f.page, buttons.at(2))->isVisibleTo(&f.page),
             "Edit stays on the bar at the narrowest window, so no gesture hides behind a menu");
}

void StartupPageTest::TheUndoButtonCutsTheNameToItsFloorAndKeepsTheWholeLabelInTheTip()
{
    ApplyModernistTheme(*qApp);

    UndoButton button;
    button.NothingToUndo(QStringLiteral("Undo"));

    QVERIFY(!button.isEnabled());
    QCOMPARE(button.text(), QStringLiteral("Undo"));
    QVERIFY(button.toolTip().isEmpty());

    button.Name(QStringLiteral("Undo: %1 added"), kLongestName);
    button.show();
    QVERIFY(QTest::qWaitForWindowExposed(&button));

    QVERIFY(button.isEnabled());
    QCOMPARE(button.toolTip(), QStringLiteral("Undo: %1 added").arg(kLongestName));
    QVERIFY2(button.minimumSizeHint().width() < button.sizeHint().width(),
             "the floor leaves the name cut, and only the preferred width asks for all of it");

    button.resize(button.minimumSizeHint().width(), button.height());
    QCoreApplication::processEvents();

    QVERIFY(button.text().startsWith(QStringLiteral("Undo: FS2")));
    QVERIFY(button.text().endsWith(QStringLiteral("… added")));
    QVERIFY(button.fontMetrics().horizontalAdvance(button.text())
            < button.fontMetrics().horizontalAdvance(button.toolTip()));

    button.resize(button.sizeHint().width(), button.height());
    QCoreApplication::processEvents();

    QCOMPARE(button.text(), QStringLiteral("Undo: %1 added").arg(kLongestName));
}

void StartupPageTest::TheMainBarOffersTheGesturesInOrderAndTheRemovedBarSwapsThem()
{
    Fixture f;
    f.viewModel.Show();

    QCOMPARE(TheButtonsOfTheBar(f.page), (QStringList{"Refresh", "Add…", "Edit…", "Remove…", "Undo", "Stop managing"}));
    QVERIFY(!ButtonSaying(f.page, "Undo")->isEnabled());
    QCOMPARE(ButtonSaying(f.page, "Stop managing")->toolTip(), QStringLiteral("Stop managing startup entries"));

    TheChip(f.page, kRemoved)->click();

    QCOMPARE(TheButtonsOfTheBar(f.page),
             (QStringList{"Refresh", "Restore entry", "Discard…", "Undo", "Stop managing"}));

    TheChip(f.page, kInTheFile)->click();

    QCOMPARE(TheButtonsOfTheBar(f.page).size(), 6);
}

void StartupPageTest::WithoutARowEditRemoveRestoreAndDiscardWait()
{
    Fixture f;
    f.entries.CarryRemoved(StartupEntry{.label = "Gone", .path = "C:/Gone/gone.exe"});
    f.viewModel.Show();

    QVERIFY(ButtonSaying(f.page, "Add…")->isEnabled());
    QVERIFY(!ButtonSaying(f.page, "Edit…")->isEnabled());
    QVERIFY(!ButtonSaying(f.page, "Remove…")->isEnabled());

    Select(TableOf(f.page), 0);

    QVERIFY(ButtonSaying(f.page, "Edit…")->isEnabled());
    QVERIFY(ButtonSaying(f.page, "Remove…")->isEnabled());

    TheChip(f.page, kRemoved)->click();

    QVERIFY(!ButtonSaying(f.page, "Restore entry")->isEnabled());
    QVERIFY(!ButtonSaying(f.page, "Discard…")->isEnabled());

    Select(RemovedOf(f.page), 0);

    QVERIFY(ButtonSaying(f.page, "Restore entry")->isEnabled());
    QVERIFY(ButtonSaying(f.page, "Discard…")->isEnabled());
}

void StartupPageTest::TheChipsCountTheFileAndTheRemovedAndSwitchTheView()
{
    Fixture f;
    f.entries.CarryRemoved(StartupEntry{.label = "Old", .path = "C:/Gone/old.exe"}, {}, f.clock.now);
    f.entries.CarryRemoved(StartupEntry{.label = "Newer", .path = "C:/Gone/newer.exe"}, {},
                           f.clock.now + std::chrono::minutes(1));
    f.viewModel.Show();

    QCOMPARE(TheChip(f.page, kInTheFile)->text(), QStringLiteral("In the file 3"));
    QCOMPARE(TheChip(f.page, kRemoved)->text(), QStringLiteral("Removed 2"));
    QVERIFY(TheChip(f.page, kInTheFile)->isChecked());
    QCOMPARE(CurrentPane(f.page), 0);

    TheChip(f.page, kRemoved)->click();

    QVERIFY(TheChip(f.page, kRemoved)->isChecked());
    QVERIFY(!TheChip(f.page, kInTheFile)->isChecked());
    QCOMPARE(CurrentPane(f.page), 3);

    const QTreeWidget* removed = RemovedOf(f.page);

    QCOMPARE(removed->topLevelItemCount(), 2);
    QCOMPARE(removed->topLevelItem(0)->text(0), QStringLiteral("Newer"));
    QCOMPARE(removed->topLevelItem(1)->text(0), QStringLiteral("Old"));
    QCOMPARE(removed->topLevelItem(0)->text(1), AsText("C:/Gone/newer.exe"));
    QCOMPARE(removed->topLevelItem(0)->text(2), AsMoment(f.clock.now + std::chrono::minutes(1)));
    QVERIFY(removed->topLevelItem(0)->data(1, QuietRole).toBool());
    QCOMPARE(removed->headerItem()->text(2), QStringLiteral("Removed"));

    TheChip(f.page, kInTheFile)->click();

    QCOMPARE(CurrentPane(f.page), 0);
}

void StartupPageTest::TheRefreshButtonCarriesTheMomentOfTheReadingAndTheBarNoLongerSaysIt()
{
    Fixture f;
    f.viewModel.Show();

    QCOMPARE(ButtonSaying(f.page, "Refresh")->toolTip(),
             QStringLiteral("startup file · read %1").arg(AsMoment(f.clock.now)));
    QVERIFY(f.page.findChild<QWidget*>(QStringLiteral("PageToolbar"))->findChildren<QLabel*>().isEmpty());
}

void StartupPageTest::OnlyOneRowIsSelectedAndAPathIsCutInTheMiddle()
{
    Fixture f;
    f.entries.CarryRemoved(StartupEntry{.label = "Gone", .path = "C:/Gone/gone.exe"});
    f.entries.CarryRemoved(StartupEntry{.label = "Gone too", .path = "C:/Gone/too.exe"});
    f.viewModel.Show();

    for (QTreeWidget* table : {TableOf(f.page), RemovedOf(f.page)})
    {
        QCOMPARE(table->selectionMode(), QAbstractItemView::SingleSelection);
        QCOMPARE(table->textElideMode(), Qt::ElideMiddle);

        Select(table, 0);
        Select(table, 1);

        QCOMPARE(table->selectedItems().size(), 1);
    }
}

void StartupPageTest::RemovingFromTheBarAsksAndTakesTheEntryOut()
{
    Fixture f;
    f.viewModel.Show();
    QSignalSpy status(&f.page, &StartupPage::StatusChanged);
    Select(TableOf(f.page), 1);

    TheBox box;
    AnswerTheNextBox(box, QStringLiteral("Remove"));
    ButtonSaying(f.page, "Remove…")->click();

    QVERIFY(box.shown);
    QCOMPARE(box.title, QStringLiteral("Remove startup entry"));
    QCOMPARE(box.asked, QStringLiteral("Remove Navigraph Simlink from the startup file?"));
    QVERIFY(box.askedMinimumWidth >= kReadableWidth);
    QCOMPARE(box.buttons, (QStringList{"Remove", "Cancel"}));
    QCOMPARE(LabelsOf(f.viewModel), (QStringList{"FlowPro", "Ghost"}));
    QCOMPARE(TableOf(f.page)->topLevelItemCount(), 2);
    QCOMPARE(TheChip(f.page, kRemoved)->text(), QStringLiteral("Removed 1"));
    QCOMPARE(ButtonStartingWith(f.page, "Undo")->toolTip(), QStringLiteral("Undo: Navigraph Simlink removed"));
    QVERIFY(ButtonStartingWith(f.page, "Undo")->isEnabled());
    QCOMPARE(status.last().first().toString(), QStringLiteral("Navigraph Simlink removed from the startup file."));
    QVERIFY2(TableOf(f.page)->selectedItems().isEmpty(), "the row that was selected is gone, and its neighbour is not");
}

void StartupPageTest::CancellingTheQuestionLeavesTheEntryWhereItWas()
{
    Fixture f;
    f.viewModel.Show();
    Select(TableOf(f.page), 1);

    TheBox box;
    AnswerTheNextBox(box, QStringLiteral("Cancel"));
    ButtonSaying(f.page, "Remove…")->click();

    QVERIFY(box.shown);
    QCOMPARE(LabelsOf(f.viewModel).size(), 3);
    QCOMPARE(f.entries.writes, std::size_t{0});
    QVERIFY(f.viewModel.Removed().empty());
}

void StartupPageTest::DeleteRemovesTheSelectedRowAfterTheQuestion()
{
    Fixture f;
    f.viewModel.Show();
    QTreeWidget* table = TableOf(f.page);
    FocusTheTable(f.page, table);
    Select(table, 0);

    TheBox box;
    AnswerTheNextBox(box, QStringLiteral("Remove"));
    QTest::keyClick(table, Qt::Key_Delete);

    QVERIFY(box.shown);
    QCOMPARE(LabelsOf(f.viewModel), (QStringList{"Navigraph Simlink", "Ghost"}));
}

void StartupPageTest::TheContextMenuOffersTheGesturesOfTheViewItIsIn()
{
    Fixture f;
    f.entries.CarryRemoved(StartupEntry{.label = "Gone", .path = "C:/Gone/gone.exe"});
    f.viewModel.Show();
    QTreeWidget* table = ShownTable(f);

    const QPoint onTheSecondRow = table->visualRect(table->model()->index(1, 1)).center();
    QStringList offered;
    TheBox box;

    ChooseFromTheNextMenu(offered, QStringLiteral("Remove…"));
    AnswerTheNextBox(box, QStringLiteral("Remove"));

    QContextMenuEvent onAnEntry(QContextMenuEvent::Mouse, onTheSecondRow,
                                table->viewport()->mapToGlobal(onTheSecondRow));
    QCoreApplication::sendEvent(table->viewport(), &onAnEntry);

    QCOMPARE(offered, (QStringList{"Edit…", "Remove…"}));
    QVERIFY(box.shown);
    QCOMPARE(LabelsOf(f.viewModel), (QStringList{"FlowPro", "Ghost"}));

    TheChip(f.page, kRemoved)->click();
    QCoreApplication::processEvents();
    QTreeWidget* removed = RemovedOf(f.page);
    const QPoint onTheFirstRemoved = removed->visualRect(removed->model()->index(0, 1)).center();
    QStringList offeredInTheRemoved;

    ChooseFromTheNextMenu(offeredInTheRemoved, QStringLiteral("Restore entry"));

    QContextMenuEvent onARemoved(QContextMenuEvent::Mouse, onTheFirstRemoved,
                                 removed->viewport()->mapToGlobal(onTheFirstRemoved));
    QCoreApplication::sendEvent(removed->viewport(), &onARemoved);

    QCOMPARE(offeredInTheRemoved, (QStringList{"Restore entry", "Discard…"}));
    QCOMPARE(f.viewModel.Lines().size(), std::size_t{3});
}

void StartupPageTest::EditOpensFromTheBarTheKeyboardAndADoubleClickOutsideTheCheck_data()
{
    QTest::addColumn<QString>("how");

    QTest::newRow("the bar") << QStringLiteral("bar");
    QTest::newRow("F2") << QStringLiteral("F2");
    QTest::newRow("a double click on the path") << QStringLiteral("double click");
}

void StartupPageTest::EditOpensFromTheBarTheKeyboardAndADoubleClickOutsideTheCheck()
{
    QFETCH(const QString, how);

    ApplyModernistTheme(*qApp);
    Fixture f;
    f.viewModel.Show();
    QTreeWidget* table = ShownTable(f);
    FocusTheTable(f.page, table);
    Select(table, 1);

    bool opened = false;
    QString title;
    StartupDraft shown;
    FillTheNextDialog(opened,
                      [&title, &shown](StartupDraftDialog& dialog)
                      {
                          title = dialog.windowTitle();
                          shown = dialog.Draft();
                      });

    if (how == QLatin1String("bar"))
    {
        ButtonSaying(f.page, "Edit…")->click();
    }
    else if (how == QLatin1String("F2"))
    {
        QTest::keyClick(table, Qt::Key_F2);
    }
    else
    {
        const QPoint onThePath = table->visualRect(table->model()->index(1, 1)).center();

        QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, onThePath);
        QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, onThePath);
    }

    QVERIFY(opened);
    QCOMPARE(title, QStringLiteral("Edit startup entry"));
    QCOMPARE(shown.label, std::string("Navigraph Simlink"));
    QCOMPARE(shown.file, kSimlink);
    QCOMPARE(f.entries.writes, std::size_t{0});
}

void StartupPageTest::ADoubleClickOnTheCheckDoesNotOpenTheEdit()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    f.viewModel.Show();
    QTreeWidget* table = ShownTable(f);

    const QRect cell = table->visualRect(table->model()->index(1, 0));
    const QRect drawn = InkOfTheCheckAlone(*table, QStyle::State_Off);
    const int drawnLeft = InkOf(PaintedCell(*table, 1, 0)).left();

    bool opened = false;
    FillTheNextDialog(opened, [](StartupDraftDialog&) {}, 10);

    const QPoint onTheBox(cell.left() + drawnLeft + drawn.width() / 2, cell.center().y());

    QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, onTheBox);
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, onTheBox);
    QTest::qWait(kTryEvery * 15);

    QVERIFY2(!opened, "a double click on the box is two ticks, and never an edit");
}

void StartupPageTest::ADoubleClickOnTheCheckOfAnotherRowNeverOpensTheEditOfTheSelectedOne()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    f.viewModel.Show();
    QTreeWidget* table = ShownTable(f);
    Select(table, 0);

    const QRect cell = table->visualRect(table->model()->index(1, 0));
    const QRect drawn = InkOfTheCheckAlone(*table, QStyle::State_Off);
    const int drawnLeft = InkOf(PaintedCell(*table, 1, 0)).left();

    bool opened = false;
    QString whose;
    FillTheNextDialog(
        opened,
        [&whose](StartupDraftDialog& dialog)
        {
            whose = QString::fromStdString(dialog.Draft().label);
        },
        10);

    const QPoint onTheBox(cell.left() + drawnLeft + drawn.width() / 2, cell.center().y());

    QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, onTheBox);
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, onTheBox);
    QTest::qWait(kTryEvery * 15);

    QVERIFY2(!opened,
             qPrintable(QStringLiteral("a double click on the box of one row opened the edit of %1").arg(whose)));
}

void StartupPageTest::AnEditThatChangesNothingSaysSoInsteadOfSaved()
{
    Fixture f;
    f.viewModel.Show();
    QSignalSpy status(&f.page, &StartupPage::StatusChanged);
    Select(TableOf(f.page), 1);

    bool opened = false;
    FillTheNextDialog(opened,
                      [](StartupDraftDialog& dialog)
                      {
                          AcceptWith(dialog, QStringLiteral("Save"));
                      });
    ButtonSaying(f.page, "Edit…")->click();

    QVERIFY(opened);
    QCOMPARE(status.last().first().toString(),
             QStringLiteral("Nothing to save: Navigraph Simlink is already like that."));
    QCOMPARE(f.entries.writes, std::size_t{0});
    QVERIFY(!ButtonStartingWith(f.page, "Undo")->isEnabled());
}

void StartupPageTest::AMovedPathSaysHowManyPresetsFollowedTheReturnPresetIncluded()
{
    Fixture f;
    Preset shortPreset;
    shortPreset.name = "Short";
    shortPreset.governsStartup = true;
    shortPreset.startupEntries = {PresetStartupEntry{.path = kSimlink, .action = PresetAction::Disable}};
    QVERIFY(f.presets.Save("msfs2024", shortPreset));

    f.viewModel.Show();
    QSignalSpy status(&f.page, &StartupPage::StatusChanged);
    Select(TableOf(f.page), 1);

    bool opened = false;
    FillTheNextDialog(opened,
                      [](StartupDraftDialog& dialog)
                      {
                          dialog.TakeTheProgram(kAny2Gsx);
                          AcceptWith(dialog, QStringLiteral("Save"));
                      });
    ButtonSaying(f.page, "Edit…")->click();

    QVERIFY(opened);
    QVERIFY(status.last().first().toString().contains(QStringLiteral("1 preset followed the new path.")));

    Preset returnPreset;
    returnPreset.name = "Back";
    returnPreset.governsStartup = true;
    returnPreset.startupEntries = {PresetStartupEntry{.path = kAny2Gsx, .action = PresetAction::Enable}};
    QVERIFY(f.presets.SaveReturnPreset("msfs2024", returnPreset));

    bool openedAgain = false;
    FillTheNextDialog(openedAgain,
                      [](StartupDraftDialog& dialog)
                      {
                          dialog.TakeTheProgram(kSimlink);
                          AcceptWith(dialog, QStringLiteral("Save"));
                      });
    Select(TableOf(f.page), 1);
    ButtonSaying(f.page, "Edit…")->click();

    QVERIFY(openedAgain);
    QVERIFY(status.last().first().toString().contains(QStringLiteral("2 preset")));
}

void StartupPageTest::AMovedPathNamesTheReturnPresetThatCouldNotFollow()
{
    Fixture f;

    Preset returnPreset;
    returnPreset.name = "Back";
    returnPreset.governsStartup = true;
    returnPreset.startupEntries = {PresetStartupEntry{.path = kSimlink, .action = PresetAction::Enable}};
    QVERIFY(f.presets.SaveReturnPreset("msfs2024", returnPreset));
    f.presets.RefuseToSaveTheReturnPreset();

    f.viewModel.Show();
    QSignalSpy status(&f.page, &StartupPage::StatusChanged);
    Select(TableOf(f.page), 1);

    bool opened = false;
    FillTheNextDialog(opened,
                      [](StartupDraftDialog& dialog)
                      {
                          dialog.TakeTheProgram(kAny2Gsx);
                          AcceptWith(dialog, QStringLiteral("Save"));
                      });
    ButtonSaying(f.page, "Edit…")->click();

    QVERIFY(opened);
    QVERIFY(status.last().first().toString().contains(
        QStringLiteral("1 preset could not follow the new path and still names the old one: "
                       "Back to the previous set.")));
}

void StartupPageTest::TheUndoButtonKeepsAnAmpersandOfTheNameAsALetter()
{
    UndoButton button;

    button.Name(QStringLiteral("Undo: %1 removed"), QStringLiteral("R&D tool"));
    button.show();
    QVERIFY(QTest::qWaitForWindowExposed(&button));

    QCOMPARE(button.toolTip(), QStringLiteral("Undo: R&D tool removed"));
    QVERIFY2(button.shortcut().isEmpty(), "an ampersand in a name is a letter and never a mnemonic");
    QVERIFY(button.text().contains(QStringLiteral("R&&D")));
}

void StartupPageTest::TheScrollBarOfBothTablesIsCappedAndTheCapFollowsTheBar()
{
    Fixture f;
    f.entries.CarryRemoved(StartupEntry{.label = "Gone", .path = "C:/Gone/gone.exe"});
    f.viewModel.Show();
    QTreeWidget* table = ShownTable(f);

    TheCapRidesWithTheScrollBar(table, table->header());

    TheChip(f.page, kRemoved)->click();
    QCoreApplication::processEvents();

    QTreeWidget* removed = RemovedOf(f.page);

    TheCapRidesWithTheScrollBar(removed, removed->header());
}

void StartupPageTest::EditingWritesWhatTheDialogSaysAndAPresetThatCouldNotFollowIsNamed()
{
    Fixture f;
    Preset locked;
    locked.name = "Locked";
    locked.governsStartup = true;
    locked.startupEntries = {PresetStartupEntry{.path = kSimlink, .action = PresetAction::Disable}};
    QVERIFY(f.presets.Save("msfs2024", locked));
    f.presets.RefuseToSave("Locked");
    f.viewModel.Show();
    QSignalSpy status(&f.page, &StartupPage::StatusChanged);
    Select(TableOf(f.page), 1);

    bool opened = false;
    FillTheNextDialog(opened,
                      [](StartupDraftDialog& dialog)
                      {
                          dialog.TakeTheProgram(kAny2Gsx);
                          dialog.findChild<QLineEdit*>(QStringLiteral("EntryCommandLine"))->setText("-x");
                          AcceptWith(dialog, QStringLiteral("Save"));
                      });
    ButtonSaying(f.page, "Edit…")->click();

    QVERIFY(opened);
    QCOMPARE(f.viewModel.Lines()[1].path, kAny2Gsx);
    QCOMPARE(f.viewModel.Lines()[1].label, std::string("Navigraph Simlink"));
    QCOMPARE(f.viewModel.Lines()[1].commandLine, std::string("-x"));
    QVERIFY(status.last().first().toString().contains(QStringLiteral("Navigraph Simlink saved")));
    QVERIFY(status.last().first().toString().contains(QStringLiteral("Locked")));
    QCOMPARE(TheChip(f.page, kRemoved)->text(), QStringLiteral("Removed 1"));
}

void StartupPageTest::AddingOpensTheDialogFromTheBarAndFromTheEmptyState()
{
    Fixture f;
    f.viewModel.Show();

    bool opened = false;
    QString title;
    FillTheNextDialog(opened,
                      [&title](StartupDraftDialog& dialog)
                      {
                          title = dialog.windowTitle();
                          dialog.TakeTheProgram(kAny2Gsx);
                          AcceptWith(dialog, QStringLiteral("Add"));
                      });
    ButtonSaying(f.page, "Add…")->click();

    QVERIFY(opened);
    QCOMPARE(title, QStringLiteral("Add startup entry"));
    QCOMPARE(LabelsOf(f.viewModel).last(), QStringLiteral("Any2GSX"));

    Fixture empty(true, false);
    empty.viewModel.Show();
    QCOMPARE(CurrentPane(empty.page), 1);

    bool openedFromTheEmptyState = false;
    FillTheNextDialog(openedFromTheEmptyState,
                      [](StartupDraftDialog& dialog)
                      {
                          dialog.TakeTheProgram(kAny2Gsx);
                          AcceptWith(dialog, QStringLiteral("Add"));
                      });
    ButtonSaying(empty.page, "Add a program…")->click();

    QVERIFY(openedFromTheEmptyState);
    QCOMPARE(CurrentPane(empty.page), 0);
    QCOMPARE(LabelsOf(empty.viewModel), (QStringList{"Any2GSX"}));
}

void StartupPageTest::UndoingWorksFromTheButtonAndFromControlZ()
{
    Fixture f;
    f.viewModel.Show();
    QTreeWidget* table = TableOf(f.page);
    FocusTheTable(f.page, table);

    QCOMPARE(f.viewModel.Remove(kSimlink).result, FileResult::Completed);
    QCOMPARE(LabelsOf(f.viewModel).size(), 2);

    QPushButton* undo = ButtonStartingWith(f.page, "Undo");

    QCOMPARE(undo->toolTip(), QStringLiteral("Undo: Navigraph Simlink removed"));

    undo->click();

    QCOMPARE(LabelsOf(f.viewModel).size(), 3);
    QCOMPARE(undo->text(), QStringLiteral("Undo"));
    QVERIFY(!undo->isEnabled());

    QCOMPARE(f.viewModel.Remove(kSimlink).result, FileResult::Completed);
    table->setFocus();
    QTest::keyClick(table, Qt::Key_Z, Qt::ControlModifier);

    QCOMPARE(LabelsOf(f.viewModel).size(), 3);

    QCOMPARE(f.viewModel.Add(StartupDraft{.label = "Any2GSX", .file = kAny2Gsx}).result, FileResult::Completed);
    QCOMPARE(undo->toolTip(), QStringLiteral("Undo: Any2GSX added"));
}

void StartupPageTest::RestoringAndDiscardingWorkFromTheRemovedView()
{
    Fixture f;
    f.viewModel.Show();
    QCOMPARE(f.viewModel.Remove(kSimlink).result, FileResult::Completed);
    f.clock.now += std::chrono::minutes(1);
    QCOMPARE(f.viewModel.Remove(kGone).result, FileResult::Completed);
    TheChip(f.page, kRemoved)->click();
    QSignalSpy status(&f.page, &StartupPage::StatusChanged);

    Select(RemovedOf(f.page), 0);
    ButtonSaying(f.page, "Restore entry")->click();

    QCOMPARE(LabelsOf(f.viewModel), (QStringList{"FlowPro", "Ghost"}));
    QCOMPARE(status.last().first().toString(), QStringLiteral("Ghost is back in the startup file."));
    QCOMPARE(RemovedOf(f.page)->topLevelItemCount(), 1);

    Select(RemovedOf(f.page), 0);
    TheBox cancelled;
    AnswerTheNextBox(cancelled, QStringLiteral("Cancel"));
    ButtonSaying(f.page, "Discard…")->click();

    QVERIFY(cancelled.shown);
    QCOMPARE(RemovedOf(f.page)->topLevelItemCount(), 1);

    TheBox confirmed;
    AnswerTheNextBox(confirmed, QStringLiteral("Discard"));
    ButtonSaying(f.page, "Discard…")->click();

    QVERIFY(confirmed.shown);
    QVERIFY(f.viewModel.Removed().empty());
    QCOMPARE(CurrentPane(f.page), 4);
    QCOMPARE(status.last().first().toString(), QStringLiteral("Navigraph Simlink discarded."));
}

void StartupPageTest::TheQuestionToRemoveFollowsTheConditionWithEachSentenceOnItsOwnLine_data()
{
    const QString takes = QStringLiteral("Removing takes its name and command line out of the startup file.");
    const QString keeps = QStringLiteral("FS Organizer keeps the entry in Removed, where you can restore it.");

    QTest::addColumn<QString>("label");
    QTest::addColumn<QStringList>("lines");

    QTest::newRow("reachable and enabled")
        << "Any2GSX on"
        << QStringList{"Disabling this entry already stops the simulator from launching the program.", takes, keeps};
    QTest::newRow("reachable and already disabled") << "Navigraph Simlink" << QStringList{takes, keeps};
    QTest::newRow("behind a disabled addon")
        << "Behind"
        << QStringList{"Its program is inside a disabled addon.", "The entry works again when you enable the addon.",
                       takes, keeps};
    QTest::newRow("broken") << "Ghost" << QStringList{"Its program no longer exists."};
    QTest::newRow("on a drive that is gone")
        << "Away" << QStringList{"Its drive is not connected right now, so the program may come back.", takes, keeps};
}

void StartupPageTest::TheQuestionToRemoveFollowsTheConditionWithEachSentenceOnItsOwnLine()
{
    QFETCH(const QString, label);
    QFETCH(const QStringList, lines);

    Fixture f;
    f.fileSystem.MarkVolumeUnavailable(kOnAbsentDrive);
    f.entries.Carry(StartupEntry{.label = "Any2GSX on", .path = kAny2Gsx, .enabled = true});
    f.entries.Carry(StartupEntry{.label = "Behind", .path = kBehindTheAddon, .enabled = true});
    f.entries.Carry(StartupEntry{.label = "Away", .path = kOnAbsentDrive, .enabled = true});
    f.viewModel.Show();

    QTreeWidget* table = TableOf(f.page);

    for (int at = 0; at < table->topLevelItemCount(); ++at)
    {
        if (table->topLevelItem(at)->text(0) == label)
        {
            Select(table, at);
        }
    }

    TheBox box;
    AnswerTheNextBox(box, QStringLiteral("Cancel"));
    ButtonSaying(f.page, "Remove…")->click();

    QVERIFY(box.shown);
    QCOMPARE(box.explained.split(QLatin1Char('\n')), lines);

    for (const QString& line : box.explained.split(QLatin1Char('\n')))
    {
        QVERIFY2(line.endsWith(QLatin1Char('.')) && line.count(QLatin1Char('.')) == 1,
                 qPrintable(QStringLiteral("'%1' is more than one sentence").arg(line)));
    }
}

void StartupPageTest::TheQuestionToDiscardSaysItIsForGood()
{
    Fixture f;
    f.viewModel.Show();
    QCOMPARE(f.viewModel.Remove(kSimlink).result, FileResult::Completed);
    TheChip(f.page, kRemoved)->click();
    Select(RemovedOf(f.page), 0);

    TheBox box;
    AnswerTheNextBox(box, QStringLiteral("Cancel"));
    ButtonSaying(f.page, "Discard…")->click();

    QVERIFY(box.shown);
    QCOMPARE(box.title, QStringLiteral("Discard removed entry"));
    QCOMPARE(box.asked, QStringLiteral("Discard Navigraph Simlink for good?"));
    QCOMPARE(box.explained.split(QLatin1Char('\n')),
             (QStringList{"FS Organizer deletes the name, path and command line it kept.", "This cannot be undone."}));
    QCOMPARE(box.buttons, (QStringList{"Discard", "Cancel"}));
    QVERIFY(box.askedMinimumWidth >= kReadableWidth);
}

void StartupPageTest::TheEmptyStatesOfBothViewsSaySoAndTheMainOneOffersToAdd()
{
    Fixture f(true, false);
    f.viewModel.Show();

    QCOMPARE(CurrentPane(f.page), 1);
    QVERIFY(ButtonSaying(f.page, "Add a program…") != nullptr);

    bool anyLabelSaysIt = false;
    for (const QLabel* label : f.page.findChildren<QLabel*>())
    {
        anyLabelSaysIt = anyLabelSaysIt || label->text() == QStringLiteral("No startup entries");
    }

    QVERIFY(anyLabelSaysIt);

    TheChip(f.page, kRemoved)->click();

    QCOMPARE(CurrentPane(f.page), 4);

    bool removedLabelSaysIt = false;
    for (const QLabel* label : f.page.findChildren<QLabel*>())
    {
        removedLabelSaysIt = removedLabelSaysIt || label->text() == QStringLiteral("No removed entries");
    }

    QVERIFY(removedLabelSaysIt);
    QVERIFY(!ButtonSaying(f.page, "Restore entry")->isEnabled());
}

QTEST_MAIN(StartupPageTest)

#include "tst_startup_page.moc"
