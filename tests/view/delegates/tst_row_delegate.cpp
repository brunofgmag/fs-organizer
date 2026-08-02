#include <QtTest/QtTest>

#include <QtGui/QHelpEvent>
#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableView>

#include "view/delegates/RowDelegate.h"
#include "view/theme/ModernistPaint.h"
#include "view/theme/ModernistTheme.h"
#include "viewmodel/RowTagRoles.h"

class RowDelegateTest : public QObject
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
    table.model.item(0, 0)->setData(QStringLiteral("o que o modelo quis dizer"), Qt::ToolTipRole);

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

    SuffixShot CellPaintedWith(const QString& suffix)
    {
        QStandardItemModel model(1, 1);
        auto* content = new QStandardItem(QStringLiteral("Community2024"));
        content->setData(suffix, QuietSuffixRole);
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
        view.viewport()->render(&shot);

        SuffixShot taken{shot.toImage(), view.visualRect(model.index(0, 0)), -1};
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
            model.setItem(row, column, new QStandardItem(QStringLiteral("celula")));
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
    view.viewport()->render(&shot);
    const QImage painted = shot.toImage();

    const int middle = first.center().y();
    const QColor inside = painted.pixelColor(first.center().x(), middle);
    const QColor atTheLeftEdge = painted.pixelColor(first.left(), middle);
    const QColor atTheSeam = painted.pixelColor(first.right(), middle);
    const QColor afterTheSeam = painted.pixelColor(second.left(), middle);

    QVERIFY2(atTheLeftEdge != inside, "a linha selecionada perdeu a borda esquerda");
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
                model.setItem(row, 0, new QStandardItem(QStringLiteral("celula")));
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

    QVERIFY2(rows.GroundOf(rows.model.index(1, 0)) != before, "a celula apontada nao acendeu");
    QVERIFY2(rows.GroundOf(rows.model.index(1, 1)) != before, "a outra celula da mesma linha nao acendeu");
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

QTEST_MAIN(RowDelegateTest)

#include "tst_row_delegate.moc"
