#include <algorithm>
#include <numeric>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QTranslator>
#include <QtTest/QtTest>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QTableWidget>

#include "application/LibraryOrganizer.h"
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
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "view/panels/EmptyState.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "view/PresetsPage.h"
#include "view/theme/ModernistTheme.h"
#include "viewmodel/PresetViewModel.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/SessionNotifier.h"
#include "tests/support/CatalogueBesideTheBuild.h"
#include "tests/support/PageFloor.h"

namespace
{
    class PresetsPageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ThePageFitsTheNarrowestWindow();
        static void TheNameTableShowsTheNamesAndTheReturnRowWholeAtTheNarrowestWindow_data();
        static void TheNameTableShowsTheNamesAndTheReturnRowWholeAtTheNarrowestWindow();
        static void TheTwoTablesKeepEveryNameWholeAndNeverScrollWhenAPresetMatches_data();
        static void TheTwoTablesKeepEveryNameWholeAndNeverScrollWhenAPresetMatches();
        static void TheColumnsKeepTheirWidthWhenAPresetBecomesSatisfied();
        static void BuildingAndTearingDownAloneDoesNotCrash();
        static void SelectingAPresetFillsThePanelPreview();
        static void ApplyingFromThePanelGoesThroughTheViewModel();
        static void TheFirstPresetStartsBelowTheTableHeaderAndNotInsideIt();
        static void TheNameTableWritesTheContentAndTheDayBesideEachPreset();
        static void FilteringHidesTheNamesThatDoNotMatchAndKeepsASelectionThatSurvives();
        static void FilteringPastTheSelectedPresetMovesTheSelectionInsteadOfStranding();
        static void ALanguageChangeReachesTheApplyButtonAndTheModeExplanation();
        static void WhatSupportsTheNameIsQuietInBothTables();
        static void TheNameTableSaysWhatEachPresetWouldChangeAndTagsTheSatisfiedOnes();
        static void TheReturnPresetSitsInItsOwnTableAndAppearsOnlyAfterAnApplication();
        static void ThePanelBreaksThePlanIntoTheSixCountsTheGlossaryFixes();
        static void TheOmittedCountAndItsButtonLeaveThePanelOutsideReplace();
        static void TheOmittedAddonsAreListedOnlyWhenAsked();
        static void TheStartupSectionStaysHiddenUntilThePresetGovernsStartup();
        static void TheStartupExplanationKeepsAReadingMeasure();
        static void TheWayBackIsTheBatchUndoAndFallsBackToTheReturnPreset();
        static void TheTwoHalvesSwapWhatTheRightSideShows();
        static void TheContentTabCountsTheEntriesOfTheSelectedPreset();
        static void ASatisfiedPresetStillShowsWhatDisableWouldChange();
        static void AFilterThatMatchesNothingLeavesNoStaleCountBehind();
        static void ChoosingTheReturnPresetSticksAndItsEntriesAreNotEditable();
        static void TheStartupTabEditsTheStartupEntriesOfAGoverningPreset();
        static void ATargetTooLongForItsColumnLosesTheMiddleAndKeepsTheFileName();
        static void AHiddenPageReadsNothingWhenTheSessionRefreshesAndReadsOnceWhenShown();
        static void AHiddenPageReadsNothingOnALanguageChangeAndReadsOnceWhenShown();
        static void AShownPageReadsOnceWhenTheSessionRefreshesAndFinishesAScanInTheSameTurn();
        static void AShownPageReloadsOnceForAChangeOfThePresetsAndTheRefreshThatFollowsIt();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kAircrafts = "D:/MSFS 2024/Aircrafts";
    constexpr auto kAddon = "D:/MSFS 2024/Aircrafts/aerosoft-crj";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kProfileId = "msfs2024";

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
        aircrafts.path = kAircrafts;
        aircrafts.children = {AddonNode(kAddon)};

        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = {std::move(aircrafts)};

        return library;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = kProfileId;
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    void ShowAndSettle(QWidget& page)
    {
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        QCoreApplication::processEvents();
    }

    void SettleTheQueuedReload()
    {
        for (int round = 0; round < 3; ++round)
        {
            QCoreApplication::processEvents();
        }
    }

    int WhatTheCellsAskFor(const QTableWidget& table, const int column)
    {
        QStyleOptionViewItem option;
        option.initFrom(&table);
        option.font = table.font();

        int asked = 0;

        for (int row = 0; row < table.rowCount(); ++row)
        {
            const QModelIndex index = table.model()->index(row, column);

            asked = std::max(asked, table.itemDelegate()->sizeHint(option, index).width());
        }

        return asked;
    }

    bool TheWholeTextFits(const QTableWidget& table, const int column)
    {
        return WhatTheCellsAskFor(table, column) <= table.columnWidth(column);
    }

    QString TheColumnsOf(const QTableWidget& table)
    {
        QStringList said;

        for (int column = 0; column < table.columnCount(); ++column)
        {
            said << QStringLiteral("column %1 is %2 px and asks %3")
                        .arg(column)
                        .arg(table.columnWidth(column))
                        .arg(WhatTheCellsAskFor(table, column));
        }

        return QStringLiteral("viewport %1 px, %2").arg(table.viewport()->width()).arg(said.join(QStringLiteral(", ")));
    }

    constexpr int kBreathingRoom = 8;
    constexpr int kBeforeTheTag = 8;
    constexpr int kPresetRow = 46;
    constexpr int kReturnTableHeight = kPresetRow + 2;
    constexpr auto kThirtyCharacters = "Transatlantic Long Haul Sector";

    QString TheTagOf(const QTableWidget& table, const int row)
    {
        return table.item(row, 0)->data(TagTextRole).toString();
    }

    QRect WhereTheNameIsWritten(const QTableWidget& table)
    {
        QStyleOptionViewItem option;
        option.initFrom(&table);
        option.widget = &table;
        option.font = table.viewport()->font();
        option.rect = QRect(0, 0, table.columnWidth(0), kPresetRow);

        return table.style()->subElementRect(QStyle::SE_ItemViewItemText, &option, &table);
    }

    int RoomForTheName(const QTableWidget& table, const int row)
    {
        const QString tag = TheTagOf(table, row);
        const int tagRoom = tag.isEmpty() ? 0 : TagSizeOf(tag, table.viewport()->font()).width() + kBeforeTheTag;

        return WhereTheNameIsWritten(table).width() - 2 * kBreathingRoom - tagRoom;
    }

    int WhatTheNameAsks(const QTableWidget& table, const int row)
    {
        return QFontMetrics(table.viewport()->font()).horizontalAdvance(table.item(row, 0)->text());
    }

    bool TheNameIsWhole(const QTableWidget& table, const int row)
    {
        return WhatTheNameAsks(table, row) <= RoomForTheName(table, row);
    }

    int WhereTheTagEnds(const QTableWidget& table, const int row)
    {
        const int tagWide = TagSizeOf(TheTagOf(table, row), table.viewport()->font()).width();

        return WhereTheNameIsWritten(table).left() + kBreathingRoom + WhatTheNameAsks(table, row) + kBeforeTheTag
            + tagWide;
    }

    int RowOf(const QTableWidget& table, const QString& name)
    {
        for (int row = 0; row < table.rowCount(); ++row)
        {
            if (table.item(row, 0)->text() == name)
            {
                return row;
            }
        }

        return -1;
    }

    QList<int> SectionsOf(const QTableWidget& table)
    {
        QList<int> sections;

        for (int column = 0; column < table.columnCount(); ++column)
        {
            sections << table.columnWidth(column);
        }

        return sections;
    }

    int WhatTheSectionsAddUpTo(const QTableWidget& table)
    {
        const QList<int> sections = SectionsOf(table);

        return std::accumulate(sections.cbegin(), sections.cend(), 0);
    }

    class MarkingTranslator final : public QTranslator
    {
    public:
        [[nodiscard]] bool isEmpty() const override
        {
            return false;
        }

        [[nodiscard]] QString translate(const char*, const char* source, const char*, int) const override
        {
            return QStringLiteral("<%1>").arg(QString::fromUtf8(source));
        }
    };

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kAircrafts);
            fileSystem.AddDirectory(kAddon);
            fileSystem.AddLink(std::filesystem::path(kCommunity) / "aerosoft-crj", kAddon);
            catalog.SetTree(kLibrary, LibraryTree());

            session.ShowActiveProfile();

            viewModel.Create(QStringLiteral("Voo de linha"));
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeProcessProbe processProbe;
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
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings{SettingsWith(Profile())};
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, settings.stored, processProbe, runner, notifier};
        FakePresetRepository presets;
        PresetService presetService{presets, service, startup.service};
        PresetViewModel viewModel{session, presetService, service, runner};
    };
}

void PresetsPageTest::TheTwoTablesKeepEveryNameWholeAndNeverScrollWhenAPresetMatches_data()
{
    QTest::addColumn<QString>("language");
    QTest::addColumn<int>("width");

    QTest::newRow("English") << QStringLiteral("en") << 1140;
    QTest::newRow("Brazilian Portuguese") << QStringLiteral("pt_BR") << kWidestAPageMayBe;
}

void PresetsPageTest::TheTwoTablesKeepEveryNameWholeAndNeverScrollWhenAPresetMatches()
{
    QFETCH(const QString, language);
    QFETCH(const int, width);

    QTranslator catalogue;

    if (language != QLatin1String("en"))
    {
        const QString file = TheCatalogueBesideTheBuild(language);

        QVERIFY2(!file.isEmpty(), "app_pt_BR.qm is not beside the build: build the release_translations target");
        QVERIFY(catalogue.load(file));
        QVERIFY(QCoreApplication::installTranslator(&catalogue));
    }

    Fixture f;
    const std::filesystem::path link = std::filesystem::path(kCommunity) / "aerosoft-crj";
    const QStringList ordinary = {QStringLiteral("Airline Ops"), QStringLiteral("GA Weekend"),
                                  QStringLiteral("Long Haul"), QStringLiteral("Everyday"),
                                  QStringLiteral("Voo de linha")};

    for (const QString& name : ordinary)
    {
        f.viewModel.Create(name);
    }

    f.fileSystem.RemoveNode(link);
    f.session.RefreshEntries();
    f.viewModel.Create(QString::fromLatin1(kThirtyCharacters));
    f.fileSystem.AddLink(link, kAddon);
    f.session.RefreshEntries();

    PresetsPage page(f.viewModel, f.notifier);
    page.resize(width, 700);
    ApplyModernistTheme(*qApp);
    ShowAndSettle(page);
    SettleTheQueuedReload();

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    auto* back = page.findChild<QTableWidget*>(QStringLiteral("PresetReturn"));
    QVERIFY(names != nullptr && back != nullptr);
    QVERIFY2(!names->wordWrap() && !back->wordWrap(), "a long name would wrap and grow its row instead of being cut");

    const QString said = TheColumnsOf(*names);

    QVERIFY2(!names->horizontalScrollBar()->isVisible(), qPrintable(said));
    QCOMPARE(WhatTheSectionsAddUpTo(*names), names->viewport()->width());

    for (const QString& name : ordinary)
    {
        const int row = RowOf(*names, name);

        QVERIFY2(row >= 0, qPrintable(name));
        QVERIFY2(!TheTagOf(*names, row).isEmpty(), qPrintable(name + QStringLiteral(" should match what is enabled")));
        QVERIFY2(TheNameIsWhole(*names, row), qPrintable(name + QStringLiteral(": ") + said));
        QVERIFY2(WhereTheTagEnds(*names, row) <= names->columnWidth(0) - kBreathingRoom,
                 qPrintable(name + QStringLiteral(": ") + said));
        QCOMPARE(names->rowHeight(row), kPresetRow);
    }

    const int longRow = RowOf(*names, QString::fromLatin1(kThirtyCharacters));

    QVERIFY(longRow >= 0);
    QVERIFY2(TheTagOf(*names, longRow).isEmpty(), "the long name should sit on a row that does not match");
    QVERIFY2(TheNameIsWhole(*names, longRow), qPrintable(said));

    names->setCurrentCell(RowOf(*names, QStringLiteral("Everyday")), 0);
    page.findChild<QRadioButton*>(QStringLiteral("ModeDisable"))->click();
    page.findChild<QPushButton*>(QStringLiteral("PresetApply"))->click();
    SettleTheQueuedReload();
    page.findChild<QRadioButton*>(QStringLiteral("ModeCumulative"))->click();
    SettleTheQueuedReload();

    QVERIFY(!back->isHidden());
    QVERIFY2(TheTagOf(*back, 0).isEmpty(), "the way back should not match what is enabled now");
    QVERIFY2(!names->horizontalScrollBar()->isVisible(), qPrintable(TheColumnsOf(*names)));

    const QString saidOfTheReturn = TheColumnsOf(*back);

    QVERIFY2(SectionsOf(*back) == SectionsOf(*names),
             qPrintable(saidOfTheReturn + QStringLiteral(" against ") + TheColumnsOf(*names)));
    QVERIFY2(TheNameIsWhole(*back, 0), qPrintable(saidOfTheReturn));
    QCOMPARE(back->rowHeight(0), kPresetRow);
    QCOMPARE(back->height(), kReturnTableHeight);

    QCoreApplication::removeTranslator(&catalogue);
}

void PresetsPageTest::TheColumnsKeepTheirWidthWhenAPresetBecomesSatisfied()
{
    Fixture f;
    const std::filesystem::path link = std::filesystem::path(kCommunity) / "aerosoft-crj";

    f.fileSystem.RemoveNode(link);
    f.session.RefreshEntries();

    PresetsPage page(f.viewModel, f.notifier);
    page.resize(kWidestAPageMayBe, 700);
    ApplyModernistTheme(*qApp);
    ShowAndSettle(page);
    SettleTheQueuedReload();

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    QVERIFY(names != nullptr);
    QVERIFY(TheTagOf(*names, 0).isEmpty());

    const QList<int> before = SectionsOf(*names);

    f.fileSystem.AddLink(link, kAddon);
    f.session.RefreshEntries();
    SettleTheQueuedReload();

    QVERIFY(!TheTagOf(*names, 0).isEmpty());
    QVERIFY2(SectionsOf(*names) == before, qPrintable(TheColumnsOf(*names)));
}

void PresetsPageTest::BuildingAndTearingDownAloneDoesNotCrash()
{
    Fixture f;
    {
        PresetsPage page(f.viewModel, f.notifier);
    }
}

void PresetsPageTest::SelectingAPresetFillsThePanelPreview()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    QVERIFY(names != nullptr);
    QCOMPARE(names->rowCount(), 1);
    QCOMPARE(names->currentRow(), 0);

    auto* apply = page.findChild<QPushButton*>(QStringLiteral("PresetApply"));
    QVERIFY(apply != nullptr);
    QVERIFY(apply->isEnabled());
    QVERIFY(apply->text().contains(QStringLiteral("enables")));
}

void PresetsPageTest::ApplyingFromThePanelGoesThroughTheViewModel()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* cumulative = page.findChild<QRadioButton*>(QStringLiteral("ModeCumulative"));
    QVERIFY(cumulative != nullptr);
    cumulative->click();

    const QSignalSpy applied(&f.viewModel, &PresetViewModel::Applied);

    auto* apply = page.findChild<QPushButton*>(QStringLiteral("PresetApply"));
    apply->click();

    QCOMPARE(applied.count(), 1);
}

void PresetsPageTest::TheFirstPresetStartsBelowTheTableHeaderAndNotInsideIt()
{
    ApplyModernistTheme(*qApp);

    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);
    page.resize(1200, 600);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    auto* entries = page.findChild<QTableWidget*>(QStringLiteral("PresetEntries"));

    QVERIFY(names != nullptr);
    QVERIFY(entries != nullptr);

    QHeaderView* heading = names->horizontalHeader();

    QCOMPARE(heading->height(), entries->horizontalHeader()->height());
    QCOMPARE(heading->font(), entries->horizontalHeader()->font());

    for (const QTableWidget* table : {names, entries})
    {
        const QHeaderView* header = table->horizontalHeader();

        QTRY_VERIFY2(
            table->viewport()->mapTo(&page, QPoint()).y() >= header->mapTo(&page, QPoint()).y() + header->height(),
            qPrintable(QStringLiteral("%1 draws its first row inside its own header").arg(table->objectName())));
    }
}

void PresetsPageTest::TheTwoHalvesSwapWhatTheRightSideShows()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* content = page.findChild<QPushButton*>(QStringLiteral("PresetContentTab"));
    auto* plan = page.findChild<QPushButton*>(QStringLiteral("PresetPlanTab"));
    auto* entries = page.findChild<QTableWidget*>(QStringLiteral("PresetEntries"));
    auto* apply = page.findChild<QPushButton*>(QStringLiteral("PresetApply"));
    QVERIFY(content != nullptr && plan != nullptr && entries != nullptr && apply != nullptr);

    QVERIFY(content->isChecked());
    QVERIFY(!plan->isChecked());

    auto* shown = qobject_cast<QStackedWidget*>(entries->parentWidget());
    QVERIFY(shown != nullptr);
    QCOMPARE(shown->currentWidget(), entries);

    plan->click();

    QVERIFY(plan->isChecked());
    QVERIFY(!content->isChecked());
    QVERIFY(shown->currentWidget() != entries);
    QVERIFY(shown->currentWidget()->isAncestorOf(apply));
}

void PresetsPageTest::TheContentTabCountsTheEntriesOfTheSelectedPreset()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* content = page.findChild<QPushButton*>(QStringLiteral("PresetContentTab"));
    auto* planFor = page.findChild<QLabel*>(QStringLiteral("PresetPlanFor"));
    QVERIFY(content != nullptr && planFor != nullptr);

    QCOMPARE(content->text(), QStringLiteral("Content · 1"));
    QCOMPARE(planFor->text(), QStringLiteral("Voo de linha"));
}

void PresetsPageTest::TheNameTableWritesTheContentAndTheDayBesideEachPreset()
{
    Fixture f;
    f.presets.SayItWasWrittenAt("Voo de linha",
                                std::chrono::system_clock::time_point(std::chrono::milliseconds(
                                    QDateTime(QDate(2026, 2, 17), QTime(9, 30)).toMSecsSinceEpoch())));

    PresetsPage page(f.viewModel, f.notifier);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    QVERIFY(names != nullptr);
    QCOMPARE(names->columnCount(), 3);
    QCOMPARE(names->rowCount(), 1);

    QCOMPARE(names->horizontalHeaderItem(0)->text(), QStringLiteral("Preset"));
    QCOMPARE(names->horizontalHeaderItem(1)->text(), QStringLiteral("Updated"));
    QCOMPARE(names->horizontalHeaderItem(2)->text(), QStringLiteral("If applied"));

    QCOMPARE(names->item(0, 0)->text(), QStringLiteral("Voo de linha"));
    QCOMPARE(names->item(0, 0)->data(SecondLineRole).toString(), QStringLiteral("1 addon · 1 category"));
    QCOMPARE(names->item(0, 1)->text(), QStringLiteral("17/02/2026"));
}

void PresetsPageTest::FilteringHidesTheNamesThatDoNotMatchAndKeepsASelectionThatSurvives()
{
    Fixture f;
    f.viewModel.Create(QStringLiteral("Bush flying"));

    PresetsPage page(f.viewModel, f.notifier);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    auto* filter = page.findChild<QLineEdit*>();
    QVERIFY(names != nullptr);
    QVERIFY(filter != nullptr);
    QCOMPARE(names->rowCount(), 2);

    const auto rowNamed = [names](const QString& name)
    {
        for (int row = 0; row < names->rowCount(); ++row)
        {
            if (names->item(row, 0)->text() == name)
            {
                return row;
            }
        }

        return -1;
    };

    const int bush = rowNamed(QStringLiteral("Bush flying"));
    const int line = rowNamed(QStringLiteral("Voo de linha"));

    names->setCurrentCell(bush, 0);

    filter->setText(QStringLiteral("bush"));

    QVERIFY(!names->isRowHidden(bush));
    QVERIFY(names->isRowHidden(line));
    QCOMPARE(names->currentRow(), bush);

    filter->clear();

    QVERIFY(!names->isRowHidden(bush));
    QVERIFY(!names->isRowHidden(line));
}

void PresetsPageTest::FilteringPastTheSelectedPresetMovesTheSelectionInsteadOfStranding()
{
    Fixture f;
    f.viewModel.Create(QStringLiteral("Bush flying"));

    PresetsPage page(f.viewModel, f.notifier);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    auto* filter = page.findChild<QLineEdit*>();

    const auto rowNamed = [names](const QString& name)
    {
        for (int row = 0; row < names->rowCount(); ++row)
        {
            if (names->item(row, 0)->text() == name)
            {
                return row;
            }
        }

        return -1;
    };

    names->setCurrentCell(rowNamed(QStringLiteral("Bush flying")), 0);

    filter->setText(QStringLiteral("linha"));

    QCOMPARE(names->currentRow(), rowNamed(QStringLiteral("Voo de linha")));
    QVERIFY(!names->isRowHidden(names->currentRow()));

    filter->setText(QStringLiteral("nothing matches this"));

    QCOMPARE(names->currentRow(), -1);
}

void PresetsPageTest::ALanguageChangeReachesTheApplyButtonAndTheModeExplanation()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);
    ShowAndSettle(page);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    QVERIFY(names != nullptr);
    names->setCurrentCell(0, 0);

    auto* apply = page.findChild<QPushButton*>(QStringLiteral("PresetApply"));
    auto* explained = page.findChild<QLabel*>(QStringLiteral("ModeExplained"));
    QVERIFY(apply != nullptr && explained != nullptr);

    const QString applyBefore = apply->text();
    const QString explainedBefore = explained->text();
    QVERIFY(!applyBefore.isEmpty());
    QVERIFY(!explainedBefore.isEmpty());

    MarkingTranslator marking;
    QCoreApplication::installTranslator(&marking);
    SettleTheQueuedReload();

    QVERIFY2(apply->text() != applyBefore, "the apply button kept its old text after the language change");
    QVERIFY2(explained->text() != explainedBefore, "the mode explanation kept its old text after the language change");

    QCoreApplication::removeTranslator(&marking);
    SettleTheQueuedReload();

    QCOMPARE(apply->text(), applyBefore);
    QCOMPARE(explained->text(), explainedBefore);
}

void PresetsPageTest::WhatSupportsTheNameIsQuietInBothTables()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    auto* entries = page.findChild<QTableWidget*>(QStringLiteral("PresetEntries"));
    QVERIFY(names != nullptr);
    QVERIFY(entries != nullptr);
    QCOMPARE(names->rowCount(), 1);
    QCOMPARE(entries->rowCount(), 1);

    QVERIFY(!names->item(0, 0)->data(QuietRole).toBool());
    QVERIFY(names->item(0, 1)->data(QuietRole).toBool());

    QVERIFY(!entries->item(0, 0)->data(QuietRole).toBool());
    QVERIFY(entries->item(0, 1)->data(QuietRole).toBool());
}

void PresetsPageTest::TheNameTableSaysWhatEachPresetWouldChangeAndTagsTheSatisfiedOnes()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);
    ShowAndSettle(page);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    QVERIFY(names != nullptr);
    QCOMPARE(names->columnCount(), 3);
    QCOMPARE(names->horizontalHeaderItem(2)->text(), QStringLiteral("If applied"));

    QCOMPARE(names->item(0, 2)->text(), QStringLiteral("0 change"));
    QCOMPARE(names->item(0, 0)->data(TagTextRole).toString(), QStringLiteral("Matches your setup"));
    QVERIFY(names->item(0, 2)->data(TagTextRole).toString().isEmpty());

    f.fileSystem.RemoveNode(std::filesystem::path(kCommunity) / "aerosoft-crj");
    f.session.RefreshEntries();
    QCoreApplication::processEvents();

    QCOMPARE(names->item(0, 2)->text(), QStringLiteral("1 change"));
    QVERIFY(names->item(0, 0)->data(TagTextRole).toString().isEmpty());
    QVERIFY(!names->item(0, 0)->text().isEmpty());
}

void PresetsPageTest::TheReturnPresetSitsInItsOwnTableAndAppearsOnlyAfterAnApplication()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);
    ShowAndSettle(page);

    auto* back = page.findChild<QTableWidget*>(QStringLiteral("PresetReturn"));
    QVERIFY(back != nullptr);
    QVERIFY(back->isHidden());

    page.findChild<QRadioButton*>(QStringLiteral("ModeCumulative"))->click();

    auto* apply = page.findChild<QPushButton*>(QStringLiteral("PresetApply"));
    QVERIFY(apply != nullptr);
    apply->click();
    QCoreApplication::processEvents();

    QVERIFY(!back->isHidden());
    QCOMPARE(back->rowCount(), 1);
    QCOMPARE(back->item(0, 0)->text(), QStringLiteral("Back to the previous set"));
    QVERIFY2(back->item(0, 0)->data(TagTextRole).toString().isEmpty(),
             "the way back says it matches through its count, a tag beside the label would cut the label");

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    QCOMPARE(names->rowCount(), 1);
    QCOMPARE(names->item(0, 0)->text(), QStringLiteral("Voo de linha"));
}

void PresetsPageTest::ThePanelBreaksThePlanIntoTheSixCountsTheGlossaryFixes()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    const auto valueOf = [&page](const QString& name)
    {
        auto* label = page.findChild<QLabel*>(name);

        return label == nullptr ? QString{} : label->text();
    };

    QCOMPARE(valueOf(QStringLiteral("PlanToEnable")), QStringLiteral("0"));
    QCOMPARE(valueOf(QStringLiteral("PlanToDisable")), QStringLiteral("0"));
    QCOMPARE(valueOf(QStringLiteral("PlanAlreadyInPlace")), QStringLiteral("1"));
    QCOMPARE(valueOf(QStringLiteral("PlanUnresolved")), QStringLiteral("0"));
    QCOMPARE(valueOf(QStringLiteral("PlanNotNamed")), QStringLiteral("0"));
    QCOMPARE(valueOf(QStringLiteral("PlanNotApplied")), QStringLiteral("0"));
}

void PresetsPageTest::TheOmittedCountAndItsButtonLeaveThePanelOutsideReplace()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);
    ShowAndSettle(page);

    auto* omitted = page.findChild<QLabel*>(QStringLiteral("PlanNotNamed"));
    auto* show = page.findChild<QPushButton*>(QStringLiteral("PresetShowOmitted"));
    QVERIFY(omitted != nullptr && show != nullptr);
    QVERIFY(!omitted->isHidden());

    auto* cumulative = page.findChild<QRadioButton*>(QStringLiteral("ModeCumulative"));
    QVERIFY(cumulative != nullptr);
    cumulative->click();
    QCoreApplication::processEvents();

    QVERIFY(omitted->isHidden());
    QVERIFY(show->isHidden());
}

void PresetsPageTest::TheOmittedAddonsAreListedOnlyWhenAsked()
{
    Fixture f;
    f.fileSystem.AddDirectory("D:/MSFS 2024/Aircrafts/fenix-a320");
    f.fileSystem.AddLink(std::filesystem::path(kCommunity) / "fenix-a320", "D:/MSFS 2024/Aircrafts/fenix-a320");

    TreeNode aircrafts;
    aircrafts.kind = TreeNodeKind::Category;
    aircrafts.path = kAircrafts;
    aircrafts.children = {AddonNode(kAddon), AddonNode("D:/MSFS 2024/Aircrafts/fenix-a320")};

    TreeNode library;
    library.kind = TreeNodeKind::Library;
    library.path = kLibrary;
    library.children = {std::move(aircrafts)};

    f.catalog.SetTree(kLibrary, library);
    f.session.ShowActiveProfile();

    PresetsPage page(f.viewModel, f.notifier);

    auto* show = page.findChild<QPushButton*>(QStringLiteral("PresetShowOmitted"));
    auto* omitted = page.findChild<QLabel*>(QStringLiteral("PlanNotNamed"));
    QVERIFY(show != nullptr && omitted != nullptr);

    QCOMPARE(omitted->text(), QStringLiteral("1"));
    QVERIFY(show->isEnabled());

    QCOMPARE(f.viewModel.Omitted(*f.viewModel.Load(QStringLiteral("Voo de linha")), ApplyMode::Replace).size(), 1);
}

void PresetsPageTest::TheStartupExplanationKeepsAReadingMeasure()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);
    page.resize(1450, 760);
    page.show();

    QVERIFY(QTest::qWaitForWindowExposed(&page));

    auto* startup = page.findChild<QPushButton*>(QStringLiteral("PresetStartupTab"));
    const QCheckBox* governs = page.findChild<QCheckBox*>(QStringLiteral("PresetGovernsStartup"));

    QVERIFY(startup != nullptr && governs != nullptr);

    startup->click();

    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);

    const EmptyState* promise = governs->parentWidget()->findChild<EmptyState*>();

    QVERIFY2(promise != nullptr,
             "the explanation is the empty state the other panels use, not a label that repeats its measure");

    const QLabel* said = promise->findChild<QLabel*>(QStringLiteral("EmptyBody"));

    QVERIFY(said != nullptr);
    QVERIFY2(said->width() <= kReadableWidth,
             "the sentence that explains the box is read, not scanned, so it keeps the measure the empty states use");
    QVERIFY2(!said->text().isEmpty(), "an explanation that is not there cannot be measured");
}

void PresetsPageTest::TheStartupSectionStaysHiddenUntilThePresetGovernsStartup()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* governs = page.findChild<QCheckBox*>(QStringLiteral("PresetGovernsStartup"));
    auto* section = page.findChild<QWidget*>(QStringLiteral("PresetStartupSection"));
    QVERIFY(governs != nullptr && section != nullptr);

    QVERIFY(!governs->isChecked());
    QVERIFY(section->isHidden());

    governs->click();

    QVERIFY(section->isHidden() == false);

    const std::optional<Preset> saved = f.viewModel.Load(QStringLiteral("Voo de linha"));
    QVERIFY(saved.has_value());
    QVERIFY(saved->governsStartup);
}

void PresetsPageTest::TheWayBackIsTheBatchUndoAndFallsBackToTheReturnPreset()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);
    ShowAndSettle(page);

    auto* back = page.findChild<QPushButton*>(QStringLiteral("PresetGoBack"));
    QVERIFY(back != nullptr);
    QVERIFY(!back->isEnabled());

    f.fileSystem.RemoveNode(std::filesystem::path(kCommunity) / "aerosoft-crj");
    f.session.RefreshEntries();
    QCoreApplication::processEvents();

    page.findChild<QRadioButton*>(QStringLiteral("ModeCumulative"))->click();
    QCoreApplication::processEvents();
    page.findChild<QPushButton*>(QStringLiteral("PresetApply"))->click();
    QCoreApplication::processEvents();

    QVERIFY(back->isEnabled());
    QCOMPARE(back->text(), QStringLiteral("Back to the previous set"));
    QVERIFY(back->toolTip().contains(QStringLiteral("just applied")));

    f.service.ForgetUndo();
    f.session.RefreshEntries();
    QCoreApplication::processEvents();

    QVERIFY(back->isEnabled());
    QCOMPARE(back->text(), QStringLiteral("Back to the previous set"));
    QVERIFY(back->toolTip().contains(QStringLiteral("before the last preset")));
}

void PresetsPageTest::ASatisfiedPresetStillShowsWhatDisableWouldChange()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);
    ShowAndSettle(page);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    QVERIFY(names != nullptr);
    QCOMPARE(names->item(0, 0)->data(TagTextRole).toString(), QStringLiteral("Matches your setup"));
    QCOMPARE(names->item(0, 2)->text(), QStringLiteral("0 change"));

    page.findChild<QRadioButton*>(QStringLiteral("ModeDisable"))->click();
    QCoreApplication::processEvents();

    QCOMPARE(names->item(0, 0)->data(TagTextRole).toString(), QStringLiteral("Matches your setup"));
    QCOMPARE(names->item(0, 2)->text(), QStringLiteral("1 change"));
}

void PresetsPageTest::AFilterThatMatchesNothingLeavesNoStaleCountBehind()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* already = page.findChild<QLabel*>(QStringLiteral("PlanAlreadyInPlace"));
    auto* filter = page.findChild<QLineEdit*>();
    QVERIFY(already != nullptr && filter != nullptr);

    QCOMPARE(already->text(), QStringLiteral("1"));

    filter->setText(QStringLiteral("nada casa com isto"));

    QCOMPARE(already->text(), QStringLiteral("0"));
    QCOMPARE(page.findChild<QPushButton*>(QStringLiteral("PresetApply"))->text(), QStringLiteral("Apply"));
}

void PresetsPageTest::ChoosingTheReturnPresetSticksAndItsEntriesAreNotEditable()
{
    Fixture f;
    f.fileSystem.AddDirectory("D:/MSFS 2024/Aircrafts/fenix-a320");
    PresetsPage page(f.viewModel, f.notifier);

    page.findChild<QRadioButton*>(QStringLiteral("ModeCumulative"))->click();
    page.findChild<QPushButton*>(QStringLiteral("PresetApply"))->click();

    auto* back = page.findChild<QTableWidget*>(QStringLiteral("PresetReturn"));
    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    auto* planFor = page.findChild<QLabel*>(QStringLiteral("PresetPlanFor"));
    auto* entries = page.findChild<QTableWidget*>(QStringLiteral("PresetEntries"));
    QVERIFY(back != nullptr && names != nullptr && planFor != nullptr && entries != nullptr);

    back->setCurrentCell(0, 0);

    QCOMPARE(planFor->text(), QStringLiteral("Back to the previous set"));
    QCOMPARE(names->currentRow(), -1);
    QCOMPARE(back->currentRow(), 0);
    QVERIFY(!entries->item(0, 2)->flags().testFlag(Qt::ItemIsUserCheckable));

    f.session.RefreshEntries();

    QCOMPARE(planFor->text(), QStringLiteral("Back to the previous set"));
    QCOMPARE(back->currentRow(), 0);
    QCOMPARE(names->currentRow(), -1);

    names->setCurrentCell(0, 0);

    QCOMPARE(planFor->text(), QStringLiteral("Voo de linha"));
    QCOMPARE(back->currentRow(), -1);
    QVERIFY(entries->item(0, 2)->flags().testFlag(Qt::ItemIsUserCheckable));
}

void PresetsPageTest::TheStartupTabEditsTheStartupEntriesOfAGoverningPreset()
{
    Fixture f;
    const std::filesystem::path launcher = "D:/MSFS 2024/Aircrafts/aerosoft-crj/launcher.exe";
    f.startup.entries.Carry(StartupEntry{.label = "Fenix", .path = launcher, .enabled = true});
    f.session.RefreshEntries();

    PresetsPage page(f.viewModel, f.notifier);

    auto* governs = page.findChild<QCheckBox*>(QStringLiteral("PresetGovernsStartup"));
    auto* startupEntries = page.findChild<QTableWidget*>(QStringLiteral("PresetStartupEntries"));
    QVERIFY(governs != nullptr && startupEntries != nullptr);

    QVERIFY(!governs->isChecked());
    QCOMPARE(startupEntries->rowCount(), 0);

    governs->click();

    QVERIFY(governs->isChecked());
    QCOMPARE(startupEntries->rowCount(), 1);
    QCOMPARE(startupEntries->item(0, 0)->text(), QStringLiteral("Fenix"));
    QCOMPARE(startupEntries->item(0, 2)->checkState(), Qt::Checked);
    QVERIFY(startupEntries->item(0, 2)->flags().testFlag(Qt::ItemIsUserCheckable));

    startupEntries->item(0, 2)->setCheckState(Qt::Unchecked);

    const std::optional<Preset> saved = f.viewModel.Load(QStringLiteral("Voo de linha"));

    QVERIFY(saved.has_value());
    QCOMPARE(saved->startupEntries.size(), std::size_t{1});
    QVERIFY(saved->startupEntries.front().action == PresetAction::Disable);
}

void PresetsPageTest::ATargetTooLongForItsColumnLosesTheMiddleAndKeepsTheFileName()
{
    Fixture f;
    const std::filesystem::path launcher = "D:/MSFS 2024/Community/brightwater-util-bridge/bin/BrightwaterBridge.exe";
    f.startup.entries.Carry(StartupEntry{.label = "Brightwater Bridge", .path = launcher, .enabled = true});
    f.session.RefreshEntries();

    PresetsPage page(f.viewModel, f.notifier);

    auto* startupEntries = page.findChild<QTableWidget*>(QStringLiteral("PresetStartupEntries"));
    QVERIFY(startupEntries != nullptr);

    page.findChild<QCheckBox*>(QStringLiteral("PresetGovernsStartup"))->click();

    const QString target = startupEntries->item(0, 1)->text();
    const QFontMetrics metrics(startupEntries->font());
    const int roomForThreeQuartersOfIt = metrics.horizontalAdvance(target) * 3 / 4;

    const QString shown = metrics.elidedText(target, startupEntries->textElideMode(), roomForThreeQuartersOfIt);

    QVERIFY2(shown != target, qPrintable(shown));
    QVERIFY2(shown.endsWith(QStringLiteral("BrightwaterBridge.exe")), qPrintable(shown));
}

void PresetsPageTest::ThePageFitsTheNarrowestWindow()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    ItFitsTheNarrowestWindow(page, "The presets page");
}

void PresetsPageTest::TheNameTableShowsTheNamesAndTheReturnRowWholeAtTheNarrowestWindow_data()
{
    QTest::addColumn<QString>("language");

    QTest::newRow("English") << QStringLiteral("en");
    QTest::newRow("Brazilian Portuguese") << QStringLiteral("pt_BR");
}

void PresetsPageTest::TheNameTableShowsTheNamesAndTheReturnRowWholeAtTheNarrowestWindow()
{
    QFETCH(const QString, language);

    QTranslator catalogue;

    if (language != QLatin1String("en"))
    {
        const QString file = TheCatalogueBesideTheBuild(language);

        QVERIFY2(!file.isEmpty(), "app_pt_BR.qm is not beside the build: build the release_translations target");
        QVERIFY(catalogue.load(file));
        QVERIFY(QCoreApplication::installTranslator(&catalogue));
    }

    Fixture f;
    f.viewModel.Create(QStringLiteral("GA Weekend"));
    f.fileSystem.RemoveNode(std::filesystem::path(kCommunity) / "aerosoft-crj");
    f.session.RefreshEntries();

    PresetsPage page(f.viewModel, f.notifier);
    page.resize(kWidestAPageMayBe, 700);
    ApplyModernistTheme(*qApp);
    ShowAndSettle(page);
    SettleTheQueuedReload();

    QVERIFY(page.width() <= kWidestAPageMayBe);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    auto* back = page.findChild<QTableWidget*>(QStringLiteral("PresetReturn"));
    QVERIFY(names != nullptr && back != nullptr);

    QStringList shown;
    for (int row = 0; row < names->rowCount(); ++row)
    {
        shown << names->item(row, 0)->text();
    }

    QVERIFY2(shown.contains(QStringLiteral("GA Weekend")), qPrintable(shown.join(QLatin1Char('|'))));

    const QString saidOfTheNames = TheColumnsOf(*names);

    QVERIFY2(TheWholeTextFits(*names, 0), qPrintable(saidOfTheNames));

    page.findChild<QRadioButton*>(QStringLiteral("ModeCumulative"))->click();
    page.findChild<QPushButton*>(QStringLiteral("PresetApply"))->click();
    SettleTheQueuedReload();

    QVERIFY(!back->isHidden());

    const QString saidOfTheReturn = TheColumnsOf(*back);

    QVERIFY2(TheWholeTextFits(*back, 0), qPrintable(saidOfTheReturn));

    QCoreApplication::removeTranslator(&catalogue);
}

void PresetsPageTest::AHiddenPageReadsNothingWhenTheSessionRefreshesAndReadsOnceWhenShown()
{
    Fixture f;

    f.presets.ForgetTheCalls();
    PresetsPage page(f.viewModel, f.notifier);

    const std::size_t listsOfOneReload = f.presets.ListCalls();
    const std::size_t loadsOfOneReload = f.presets.LoadCalls();

    QVERIFY(listsOfOneReload > 0);

    f.presets.ForgetTheCalls();

    emit f.notifier.Refreshed();
    emit f.notifier.ScanFinished();
    QCoreApplication::processEvents();

    QCOMPARE(f.presets.ListCalls(), std::size_t{0});
    QCOMPARE(f.presets.LoadCalls(), std::size_t{0});

    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    QCoreApplication::processEvents();

    QCOMPARE(f.presets.ListCalls(), listsOfOneReload);
    QCOMPARE(f.presets.LoadCalls(), loadsOfOneReload);

    f.presets.ForgetTheCalls();
    page.hide();
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    QCoreApplication::processEvents();

    QCOMPARE(f.presets.ListCalls(), std::size_t{0});
}

void PresetsPageTest::AHiddenPageReadsNothingOnALanguageChangeAndReadsOnceWhenShown()
{
    Fixture f;

    f.presets.ForgetTheCalls();
    PresetsPage page(f.viewModel, f.notifier);

    const std::size_t listsOfOneReload = f.presets.ListCalls();
    const std::size_t loadsOfOneReload = f.presets.LoadCalls();

    QVERIFY(listsOfOneReload > 0);

    f.presets.ForgetTheCalls();

    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(&page, &languageChange);
    QCoreApplication::processEvents();

    QCOMPARE(f.presets.ListCalls(), std::size_t{0});
    QCOMPARE(f.presets.LoadCalls(), std::size_t{0});

    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    QCoreApplication::processEvents();

    QCOMPARE(f.presets.ListCalls(), listsOfOneReload);
    QCOMPARE(f.presets.LoadCalls(), loadsOfOneReload);
}

void PresetsPageTest::AShownPageReadsOnceWhenTheSessionRefreshesAndFinishesAScanInTheSameTurn()
{
    Fixture f;

    f.presets.ForgetTheCalls();
    PresetsPage page(f.viewModel, f.notifier);

    const std::size_t listsOfOneReload = f.presets.ListCalls();
    const std::size_t loadsOfOneReload = f.presets.LoadCalls();

    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    QCoreApplication::processEvents();

    f.presets.ForgetTheCalls();

    emit f.notifier.Refreshed();
    emit f.notifier.ScanFinished();

    QCOMPARE(f.presets.ListCalls(), std::size_t{0});

    QCoreApplication::processEvents();

    QCOMPARE(f.presets.ListCalls(), listsOfOneReload);
    QCOMPARE(f.presets.LoadCalls(), loadsOfOneReload);
}

void PresetsPageTest::AShownPageReloadsOnceForAChangeOfThePresetsAndTheRefreshThatFollowsIt()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    QCoreApplication::processEvents();

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    QVERIFY(names != nullptr);
    QCOMPARE(names->rowCount(), 1);

    f.viewModel.Create(QStringLiteral("Outro voo"));

    f.presets.ForgetTheCalls();

    emit f.notifier.Refreshed();
    QCoreApplication::processEvents();

    QCOMPARE(names->rowCount(), 2);
    QCOMPARE(f.presets.ListCalls(), std::size_t{1});
}

QTEST_MAIN(PresetsPageTest)

#include "tst_presets_page.moc"
