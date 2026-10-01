#include <QtTest/QtTest>

#include <QtGui/QHelpEvent>
#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableView>
#include <QtWidgets/QToolTip>

#include "view/delegates/RowDelegate.h"
#include "view/theme/ModernistPaint.h"
#include "view/theme/ModernistTheme.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    class RowDelegateTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ATextTooWideForItsColumnAnswersWithATooltip();
        static void ATextThatFitsItsColumnIsLeftWithoutATooltip();
        static void ATooltipTheModelSuppliesWinsOverTheOneMeasuredFromTheColumn();
        static void ASelectedRowInATableIsOutlinedOnceAndNotCellByCell();
        static void PointingAtOneCellLightsUpTheWholeRowAndNoOther();
        static void TheGroundGoesBackWhenThePointerLeaves();
        static void AScreenThatAsksForShorterRowsGetsThemWithoutLosingTheRest();
        static void TheQuietSuffixIsLaidAfterTheTextAndNotOverIt();
        static void ARowTheModelCallsAlarmingIsGroundedInTheAlertColour();
        static void ASecondLineTooWideForItsColumnAnswersWithATooltipCarryingBothLines();
        static void ASecondLineThatFitsLeavesTheCellWithoutATooltip();
        static void TheSecondLineIsCutInTheMiddleSoBothEndsSurvive();
        static void ACellThatOnlyDrawsATagDoesNotAskForTheWidthOfTheTextItReplaced();
        static void TheTagIsLaidAfterTheTextTheViewportDrawsAndNotOverIt();
        static void ATextCutInTheFontTheViewportDrawsAnswersWithATooltip();
        static void TheWidthACellAsksForFitsTheFontTheViewportDraws();
        static void AnEmphasisedNameIsCutInTheWeightItIsDrawnIn();
        static void TheTwoLinesOfALeadingCellAreABlockCentredInTheRow();
        static void ACellBesideTwoLinesOfTextDrawsOnTheFirstLine();
        static void WithoutALeadingCellTheSecondLineStillStartsAtTheMiddleOfTheRow();
    };
}

namespace
{
    constexpr auto kLongName = "tfdidesign-aircraft-md-11fge-md-11fpw-fedex-pack";

    struct Table
    {
        QStandardItemModel model{1, 1};
        QTableView view;
        RowDelegate delegate;

        explicit Table(const QString& text)
        {
            model.setItem(0, 0, new QStandardItem(text));
            view.setModel(&model);
            view.setItemDelegate(&delegate);
            view.verticalHeader()->setVisible(false);
            view.resize(1400, 120);
            view.show();
            static_cast<void>(QTest::qWaitForWindowExposed(&view));
        }

        [[nodiscard]] int RoomEnoughFor(const QString& text) const
        {
            return QFontMetrics(view.font()).horizontalAdvance(text) + 60;
        }

        [[nodiscard]] bool AsksForATooltipOn(const int columnWidth)
        {
            view.horizontalHeader()->resizeSection(0, columnWidth);

            const QModelIndex cell = model.index(0, 0);

            QStyleOptionViewItem option;
            option.initFrom(&view);
            option.widget = &view;
            option.rect = view.visualRect(cell);
            option.font = view.font();

            QHelpEvent event(QEvent::ToolTip, QPoint(4, 4), view.viewport()->mapToGlobal(QPoint(4, 4)));

            return delegate.helpEvent(&event, &view, option, cell);
        }
    };
}

void RowDelegateTest::AScreenThatAsksForShorterRowsGetsThemWithoutLosingTheRest()
{
    QStandardItemModel model(1, 1);
    model.setItem(0, 0, new QStandardItem(QStringLiteral("aerosoft-crj")));

    QStyleOptionViewItem item;
    item.font = QApplication::font();
    item.fontMetrics = QFontMetrics(item.font);

    const RowDelegate asShipped;
    const int tall = asShipped.sizeHint(item, model.index(0, 0)).height();

    RowDelegate shortened;
    shortened.KeepRowsAtLeast(0);
    const int shortest = shortened.sizeHint(item, model.index(0, 0)).height();

    QVERIFY(shortest < tall);
    QCOMPARE(asShipped.sizeHint(item, model.index(0, 0)).width(), shortened.sizeHint(item, model.index(0, 0)).width());
}

void RowDelegateTest::ATextTooWideForItsColumnAnswersWithATooltip()
{
    Table table{QString::fromLatin1(kLongName)};

    QVERIFY(table.AsksForATooltipOn(90));
}

void RowDelegateTest::ATextThatFitsItsColumnIsLeftWithoutATooltip()
{
    Table table{QString::fromLatin1(kLongName)};

    QVERIFY(!table.AsksForATooltipOn(table.RoomEnoughFor(QString::fromLatin1(kLongName))));
}

void RowDelegateTest::ATooltipTheModelSuppliesWinsOverTheOneMeasuredFromTheColumn()
{
    Table table{QStringLiteral("curto")};
    table.model.item(0, 0)->setData(QStringLiteral("what the model meant to say"), Qt::ToolTipRole);

    QVERIFY(table.AsksForATooltipOn(table.RoomEnoughFor(QStringLiteral("curto"))));
}

namespace
{
    struct SuffixShot
    {
        QImage painted;
        QRect cell;
        int inkEndsAt = -1;
    };

    SuffixShot CellPaintedWith(const QString& suffix, const QString& tag = QString())
    {
        QStandardItemModel model(1, 1);
        auto* content = new QStandardItem(QStringLiteral("Community2024"));
        content->setData(suffix, QuietSuffixRole);
        content->setData(tag, TagTextRole);
        model.setItem(0, 0, content);

        QTableView view;
        RowDelegate delegate;
        view.setModel(&model);
        view.setItemDelegate(&delegate);
        view.verticalHeader()->setVisible(false);
        view.horizontalHeader()->setVisible(false);
        view.setShowGrid(false);
        view.resize(520, 80);
        view.horizontalHeader()->resizeSection(0, 500);
        view.show();

        if (!QTest::qWaitForWindowExposed(&view))
        {
            return {};
        }

        QPixmap shot(view.viewport()->size());
        shot.fill(Qt::transparent);
        view.viewport()->render(&shot);

        SuffixShot taken{.painted = shot.toImage(), .cell = view.visualRect(model.index(0, 0)), .inkEndsAt = -1};
        const QColor ground = taken.painted.pixelColor(taken.cell.right() - 1, taken.cell.center().y());

        for (int x = taken.cell.left(); x <= taken.cell.right(); ++x)
        {
            for (int y = taken.cell.top(); y <= taken.cell.bottom(); ++y)
            {
                if (taken.painted.pixelColor(x, y) != ground)
                {
                    taken.inkEndsAt = x;
                    break;
                }
            }
        }

        return taken;
    }
}

void RowDelegateTest::TheQuietSuffixIsLaidAfterTheTextAndNotOverIt()
{
    ApplyModernistTheme(*qApp);

    const SuffixShot alone = CellPaintedWith(QString());
    const SuffixShot suffixed = CellPaintedWith(QStringLiteral("fixado"));

    QVERIFY(alone.inkEndsAt > alone.cell.left());
    QVERIFY(suffixed.inkEndsAt > alone.inkEndsAt);

    for (int x = alone.cell.left(); x <= alone.inkEndsAt; ++x)
    {
        for (int y = alone.cell.top(); y <= alone.cell.bottom(); ++y)
        {
            QCOMPARE(suffixed.painted.pixelColor(x, y), alone.painted.pixelColor(x, y));
        }
    }
}

void RowDelegateTest::ARowTheModelCallsAlarmingIsGroundedInTheAlertColour()
{
    ApplyModernistTheme(*qApp);

    QStandardItemModel model(2, 2);
    for (int row = 0; row < 2; ++row)
    {
        for (int column = 0; column < 2; ++column)
        {
            auto* content = new QStandardItem(QStringLiteral("entrada"));
            content->setData(row == 0, AlarmingRole);
            model.setItem(row, column, content);
        }
    }

    QTableView view;
    RowDelegate delegate;
    view.setModel(&model);
    view.setItemDelegate(&delegate);
    view.verticalHeader()->setVisible(false);
    view.horizontalHeader()->setVisible(false);
    view.setShowGrid(false);
    view.resize(420, 140);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QPixmap shot(view.viewport()->size());
    shot.fill(Qt::transparent);
    view.viewport()->render(&shot);
    const QImage painted = shot.toImage();

    const QRect alarming = view.visualRect(model.index(0, 1));
    const QRect quiet = view.visualRect(model.index(1, 1));

    QCOMPARE(painted.pixelColor(alarming.right() - 2, alarming.center().y()), AlarmingRowGround());
    QVERIFY(painted.pixelColor(quiet.right() - 2, quiet.center().y()) != AlarmingRowGround());
}

void RowDelegateTest::ASelectedRowInATableIsOutlinedOnceAndNotCellByCell()
{
    ApplyModernistTheme(*qApp);

    QStandardItemModel model(2, 3);
    for (int row = 0; row < 2; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            model.setItem(row, column, new QStandardItem(QStringLiteral("cell")));
        }
    }

    QTableView view;
    RowDelegate delegate;
    view.setModel(&model);
    view.setItemDelegate(&delegate);
    view.setSelectionBehavior(QAbstractItemView::SelectRows);
    view.verticalHeader()->setVisible(false);
    view.setShowGrid(false);
    view.resize(420, 140);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    view.selectRow(0);
    QTest::qWait(60);

    const QRect first = view.visualRect(model.index(0, 0));
    const QRect second = view.visualRect(model.index(0, 1));
    QVERIFY(first.width() > 4);
    QVERIFY(second.left() > first.left());

    QPixmap shot(view.viewport()->size());
    shot.fill(Qt::transparent);
    view.viewport()->render(&shot);
    const QImage painted = shot.toImage();

    const int middle = first.center().y();
    const QColor inside = painted.pixelColor(first.center().x(), middle);
    const QColor atTheLeftEdge = painted.pixelColor(first.left(), middle);
    const QColor atTheSeam = painted.pixelColor(first.right(), middle);
    const QColor afterTheSeam = painted.pixelColor(second.left(), middle);

    QVERIFY2(atTheLeftEdge != inside, "the selected row lost its left edge");
    QCOMPARE(atTheSeam, inside);
    QCOMPARE(afterTheSeam, inside);
}

namespace
{
    struct Rows
    {
        QStandardItemModel model{3, 2};
        QTableView view;
        RowDelegate* delegate = nullptr;

        Rows()
        {
            for (int row = 0; row < 3; ++row)
            {
                model.setItem(row, 0, new QStandardItem(QStringLiteral("cell")));
                model.setItem(row, 1, new QStandardItem(QStringLiteral("outra")));
            }

            view.setModel(&model);
            delegate = new RowDelegate(&view);
            view.setItemDelegate(delegate);
            view.verticalHeader()->setVisible(false);
            view.setShowGrid(false);
            view.resize(320, 160);
            view.show();
            static_cast<void>(QTest::qWaitForWindowExposed(&view));
        }

        void PointAt(const QModelIndex& cell) const
        {
            const QPoint spot = view.visualRect(cell).center();
            QMouseEvent moved(QEvent::MouseMove, QPointF(spot), view.viewport()->mapToGlobal(spot), Qt::NoButton,
                              Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(view.viewport(), &moved);
        }

        void PointAway() const
        {
            QEvent left(QEvent::Leave);
            QCoreApplication::sendEvent(view.viewport(), &left);
        }

        [[nodiscard]] QColor GroundOf(const QModelIndex& cell) const
        {
            QPixmap shot(view.viewport()->size());
            shot.fill(Qt::transparent);
            view.viewport()->render(&shot);

            const QRect where = view.visualRect(cell);

            return shot.toImage().pixelColor(where.right() - 2, where.center().y());
        }
    };
}

void RowDelegateTest::PointingAtOneCellLightsUpTheWholeRowAndNoOther()
{
    ApplyModernistTheme(*qApp);

    Rows rows;
    const QColor before = rows.GroundOf(rows.model.index(1, 1));

    rows.PointAt(rows.model.index(1, 0));

    QVERIFY2(rows.GroundOf(rows.model.index(1, 0)) != before, "the cell under the pointer did not light up");
    QVERIFY2(rows.GroundOf(rows.model.index(1, 1)) != before, "the other cell in the same row did not light up");
    QCOMPARE(rows.GroundOf(rows.model.index(0, 0)), before);
    QCOMPARE(rows.GroundOf(rows.model.index(2, 0)), before);
}

void RowDelegateTest::TheGroundGoesBackWhenThePointerLeaves()
{
    ApplyModernistTheme(*qApp);

    Rows rows;
    const QColor before = rows.GroundOf(rows.model.index(1, 0));

    rows.PointAt(rows.model.index(1, 0));
    QVERIFY(rows.GroundOf(rows.model.index(1, 0)) != before);

    rows.PointAway();

    QCOMPARE(rows.GroundOf(rows.model.index(1, 0)), before);
}

namespace
{
    struct CellWithTwoLines
    {
        QStandardItemModel model{1, 1};
        QTableView view;
        RowDelegate delegate;

        CellWithTwoLines(const QString& text, const QString& second, const int columnWidth)
        {
            auto* content = new QStandardItem(text);
            content->setData(second, SecondLineRole);
            model.setItem(0, 0, content);

            view.setModel(&model);
            view.setItemDelegate(&delegate);
            view.verticalHeader()->setVisible(false);
            view.horizontalHeader()->setVisible(false);
            view.setShowGrid(false);
            view.resize(columnWidth + 40, 120);
            view.show();
            static_cast<void>(QTest::qWaitForWindowExposed(&view));
            view.horizontalHeader()->resizeSection(0, columnWidth);
        }

        [[nodiscard]] bool AsksForATooltip()
        {
            const QModelIndex cell = model.index(0, 0);

            QStyleOptionViewItem option;
            option.initFrom(&view);
            option.widget = &view;
            option.rect = view.visualRect(cell);
            option.font = view.font();

            QHelpEvent event(QEvent::ToolTip, QPoint(4, 4), view.viewport()->mapToGlobal(QPoint(4, 4)));

            return delegate.helpEvent(&event, &view, option, cell);
        }

        [[nodiscard]] QImage Painted()
        {
            QPixmap shot(view.viewport()->size());
            shot.fill(Qt::transparent);
            view.viewport()->render(&shot);

            return shot.toImage();
        }
    };

    constexpr auto kLongPath = R"(D:\MSFS 2024\Utils\a-folder-with-a-name-far-too-long-for-the-column\)";
}

void RowDelegateTest::ASecondLineTooWideForItsColumnAnswersWithATooltipCarryingBothLines()
{
    CellWithTwoLines cell{QStringLiteral("p42-util-flow-pro"), QString::fromLatin1(kLongPath), 120};

    QVERIFY2(cell.AsksForATooltip(), "the path under the name was cut with no way to read the rest");
    QCOMPARE(QToolTip::text(), QStringLiteral("p42-util-flow-pro\n") + QString::fromLatin1(kLongPath));
}

void RowDelegateTest::ASecondLineThatFitsLeavesTheCellWithoutATooltip()
{
    CellWithTwoLines cell{QStringLiteral("p42"), QStringLiteral("D:\\Utils"), 600};

    QVERIFY(!cell.AsksForATooltip());
}

void RowDelegateTest::TheSecondLineIsCutInTheMiddleSoBothEndsSurvive()
{
    ApplyModernistTheme(*qApp);

    CellWithTwoLines alpha{QStringLiteral("p42"), QString::fromLatin1(kLongPath) + QStringLiteral("alpha"), 150};
    CellWithTwoLines omega{QStringLiteral("p42"), QString::fromLatin1(kLongPath) + QStringLiteral("omega"), 150};

    QVERIFY2(alpha.Painted() != omega.Painted(),
             "two paths that differ only at the end were painted the same, so the end was the part thrown away");
}

void RowDelegateTest::ACellThatOnlyDrawsATagDoesNotAskForTheWidthOfTheTextItReplaced()
{
    const QString tag = QStringLiteral("Divergent");

    QStandardItemModel model(2, 1);

    auto* spelled = new QStandardItem(tag);
    spelled->setData(tag, TagTextRole);
    model.setItem(0, 0, spelled);

    auto* bare = new QStandardItem(QString());
    bare->setData(tag, TagTextRole);
    model.setItem(1, 0, bare);

    QStyleOptionViewItem item;
    item.font = QApplication::font();
    item.fontMetrics = QFontMetrics(item.font);

    const RowDelegate delegate;

    QCOMPARE(delegate.sizeHint(item, model.index(0, 0)).width(), delegate.sizeHint(item, model.index(1, 0)).width());
}

namespace
{
    class ItemViewsInTheirOwnFont
    {
    public:
        ItemViewsInTheirOwnFont()
        {
            QFont smaller = QApplication::font();
            smaller.setPointSizeF(QApplication::font().pointSizeF() * 0.7);
            QApplication::setFont(smaller, "QAbstractItemView");
        }

        ItemViewsInTheirOwnFont(const ItemViewsInTheirOwnFont&) = delete;
        ItemViewsInTheirOwnFont& operator=(const ItemViewsInTheirOwnFont&) = delete;

        ~ItemViewsInTheirOwnFont()
        {
            QApplication::setFont(QApplication::font());
        }
    };
}

void RowDelegateTest::TheTagIsLaidAfterTheTextTheViewportDrawsAndNotOverIt()
{
    ApplyModernistTheme(*qApp);
    const ItemViewsInTheirOwnFont themed;

    const SuffixShot alone = CellPaintedWith(QString());
    const SuffixShot tagged = CellPaintedWith(QString(), QStringLiteral("Your library"));

    QVERIFY(alone.inkEndsAt > alone.cell.left());
    QVERIFY(tagged.inkEndsAt > alone.inkEndsAt);

    for (int x = alone.cell.left(); x <= alone.inkEndsAt; ++x)
    {
        for (int y = alone.cell.top(); y <= alone.cell.bottom(); ++y)
        {
            QCOMPARE(tagged.painted.pixelColor(x, y), alone.painted.pixelColor(x, y));
        }
    }
}

void RowDelegateTest::ATextCutInTheFontTheViewportDrawsAnswersWithATooltip()
{
    ApplyModernistTheme(*qApp);
    const ItemViewsInTheirOwnFont themed;

    Table table{QString::fromLatin1(kLongName)};
    QVERIFY(table.view.font() != table.view.viewport()->font());

    const int drawnWide = QFontMetrics(table.view.viewport()->font()).horizontalAdvance(QString::fromLatin1(kLongName));
    const int measuredWide = QFontMetrics(table.view.font()).horizontalAdvance(QString::fromLatin1(kLongName));
    QVERIFY(measuredWide + 30 < drawnWide);

    QVERIFY2(table.AsksForATooltipOn(measuredWide + 30), "the name was cut on screen with no way to read the rest");
}

void RowDelegateTest::TheWidthACellAsksForFitsTheFontTheViewportDraws()
{
    ApplyModernistTheme(*qApp);
    const ItemViewsInTheirOwnFont themed;

    Table table{QString::fromLatin1(kLongName)};
    QVERIFY(table.view.font() != table.view.viewport()->font());

    QStyleOptionViewItem option;
    option.initFrom(&table.view);
    option.widget = &table.view;
    option.font = table.view.font();

    const int drawnWide = QFontMetrics(table.view.viewport()->font()).horizontalAdvance(QString::fromLatin1(kLongName));

    QVERIFY(table.delegate.sizeHint(option, table.model.index(0, 0)).width() > drawnWide);
}

void RowDelegateTest::AnEmphasisedNameIsCutInTheWeightItIsDrawnIn()
{
    Table table{QString::fromLatin1(kLongName)};

    int tightest = 90;
    while (table.AsksForATooltipOn(tightest))
    {
        ++tightest;
    }

    table.model.item(0, 0)->setData(true, EmphasisRole);

    QVERIFY2(table.AsksForATooltipOn(tightest), "the name was cut in its heavier weight with no way to read the rest");
}

namespace
{
    constexpr int kTwoLinesRow = 46;
    constexpr int kBetweenTheTwoLines = 4;
    constexpr auto kCapitals = "HHHH";

    struct Band
    {
        int top = 0;
        int bottom = 0;
    };

    struct RowOfTwoCells
    {
        QStandardItemModel model{1, 2};
        QTableView view;
        RowDelegate delegate;
        QImage painted;

        RowOfTwoCells(const bool firstCellLeads, const QString& secondLine)
        {
            auto* leading = new QStandardItem(QString::fromLatin1(kCapitals));
            leading->setData(secondLine, SecondLineRole);
            model.setItem(0, 0, leading);
            model.setItem(0, 1, new QStandardItem(QString::fromLatin1(kCapitals)));

            delegate.KeepRowsAtLeast(kTwoLinesRow);

            if (firstCellLeads)
            {
                delegate.LetTheFirstCellLeadTheRow();
            }

            view.setModel(&model);
            view.setItemDelegate(&delegate);
            view.verticalHeader()->setVisible(false);
            view.verticalHeader()->setDefaultSectionSize(kTwoLinesRow);
            view.horizontalHeader()->setVisible(false);
            view.setShowGrid(false);
            view.resize(440, 80);
            view.horizontalHeader()->resizeSection(0, 200);
            view.horizontalHeader()->resizeSection(1, 200);
            view.show();
            static_cast<void>(QTest::qWaitForWindowExposed(&view));

            QPixmap shot(view.viewport()->size());
            shot.fill(Qt::transparent);
            view.viewport()->render(&shot);
            painted = shot.toImage();
        }

        [[nodiscard]] QList<Band> InkIn(const int column) const
        {
            const QRect cell = view.visualRect(model.index(0, column));
            const QColor ground = painted.pixelColor(cell.right() - 1, cell.top() + 1);

            QList<Band> bands;

            for (int y = cell.top(); y <= cell.bottom(); ++y)
            {
                bool inked = false;

                for (int x = cell.left(); x <= cell.right() && !inked; ++x)
                {
                    inked = painted.pixelColor(x, y) != ground;
                }

                if (inked && (bands.isEmpty() || bands.last().bottom != y - 1))
                {
                    bands.append({.top = y, .bottom = y});
                }
                else if (inked)
                {
                    bands.last().bottom = y;
                }
            }

            return bands;
        }

        [[nodiscard]] int Line() const
        {
            return QFontMetrics(view.viewport()->font()).height();
        }
    };
}

void RowDelegateTest::TheTwoLinesOfALeadingCellAreABlockCentredInTheRow()
{
    ApplyModernistTheme(*qApp);

    const RowOfTwoCells alone(true, QString());
    const RowOfTwoCells twoLines(true, QString::fromLatin1(kCapitals));

    const QList<Band> single = alone.InkIn(0);
    const QList<Band> both = twoLines.InkIn(0);

    QCOMPARE(single.size(), 1);
    QCOMPARE(both.size(), 2);

    const int line = twoLines.Line();
    QCOMPARE(both[1].top - both[0].top, line + kBetweenTheTwoLines);

    const int above = (kTwoLinesRow - 2 * line - kBetweenTheTwoLines) / 2;
    const int centred = (kTwoLinesRow - line) / 2;

    QVERIFY2(qAbs((both[0].top - single[0].top) - (above - centred)) <= 1,
             qPrintable(QStringLiteral("the first line starts at %1 and a lone line at %2 in a row of %3 with a line "
                                       "of %4")
                            .arg(both[0].top)
                            .arg(single[0].top)
                            .arg(kTwoLinesRow)
                            .arg(line)));
}

void RowDelegateTest::ACellBesideTwoLinesOfTextDrawsOnTheFirstLine()
{
    ApplyModernistTheme(*qApp);

    const RowOfTwoCells row(true, QString::fromLatin1(kCapitals));

    const QList<Band> leading = row.InkIn(0);
    const QList<Band> beside = row.InkIn(1);

    QCOMPARE(leading.size(), 2);
    QCOMPARE(beside.size(), 1);
    QCOMPARE(beside[0].top, leading[0].top);
    QCOMPARE(beside[0].bottom, leading[0].bottom);
}

void RowDelegateTest::WithoutALeadingCellTheSecondLineStillStartsAtTheMiddleOfTheRow()
{
    ApplyModernistTheme(*qApp);

    const RowOfTwoCells row(false, QString::fromLatin1(kCapitals));

    const QList<Band> leading = row.InkIn(0);
    const QList<Band> beside = row.InkIn(1);
    const int middle = row.view.visualRect(row.model.index(0, 0)).center().y();

    QCOMPARE(leading.size(), 2);
    QCOMPARE(beside.size(), 1);
    QVERIFY(leading[0].bottom <= middle);
    QVERIFY(leading[1].top >= middle);
    QVERIFY(beside[0].top > leading[0].top);
}

QTEST_MAIN(RowDelegateTest)

#include "tst_row_delegate.moc"
