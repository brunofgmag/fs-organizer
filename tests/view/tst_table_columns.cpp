#include <QtTest/QtTest>

#include <QtGui/QStandardItemModel>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableView>

#include "view/TableColumns.h"

namespace
{
    class TableColumnsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheColumnsFillTheTableWithoutLeavingAStripOnTheRight();
        static void WideningOneColumnWidensThatColumnAndNoOther();
        static void NarrowingOneColumnIsAbsorbedInsteadOfLeavingAGap();
        static void EveryColumnButTheLastCanBeDragged();
        static void TheColumnThatTakesTheSlackIsDraggedLikeAnyOther();
        static void ADraggedColumnKeepsItsLeftEdgeAndMovesTheOneAfterIt();
        static void TheLastColumnRefusesTheUserBecauseItHasNothingToTradeWith();
        static void NoColumnIsSqueezedNarrowerThanItsOwnTitle();
        static void AWiderWindowGoesToTheColumnThatTakesTheSlack();
        static void ContentThatArrivesAfterTheHelperIsMeasuredJustTheSame();
        static void ContentThatChangesWithoutNewRowsIsMeasuredAgain();
        static void TheSlackCanBeGivenToAChosenColumnInsteadOfTheLast();
    };
}

namespace
{
    constexpr int kColumns = 4;
    constexpr int kSlack = kColumns - 1;

    struct Table
    {
        QStandardItemModel model{6, kColumns};
        QTableView view;

        Table()
        {
            for (int row = 0; row < model.rowCount(); ++row)
            {
                for (int column = 0; column < kColumns; ++column)
                {
                    model.setItem(row, column, new QStandardItem(QStringLiteral("cell %1").arg(column)));
                }
            }

            view.setModel(&model);
            view.resize(800, 300);
            LetTheColumnsBeDraggedAndStillFillTheTable(&view);
            view.show();
            static_cast<void>(QTest::qWaitForWindowExposed(&view));
        }

        [[nodiscard]] QHeaderView* Header() const
        {
            return view.horizontalHeader();
        }

        [[nodiscard]] int Width(const int column) const
        {
            return Header()->sectionSize(column);
        }

        [[nodiscard]] int TotalWidth() const
        {
            int total = 0;
            for (int column = 0; column < kColumns; ++column)
            {
                total += Width(column);
            }

            return total;
        }

        void DragTo(const int column, const int width) const
        {
            Header()->resizeSection(column, width);
        }
    };
}

void TableColumnsTest::TheColumnsFillTheTableWithoutLeavingAStripOnTheRight()
{
    const Table table;

    QCOMPARE(table.TotalWidth(), table.view.viewport()->width());
}

void TableColumnsTest::WideningOneColumnWidensThatColumnAndNoOther()
{
    const Table table;
    const int before = table.Width(0);

    table.DragTo(0, before + 120);

    QCOMPARE(table.Width(0), before + 120);
    QCOMPARE(table.TotalWidth(), table.view.viewport()->width());
}

void TableColumnsTest::NarrowingOneColumnIsAbsorbedInsteadOfLeavingAGap()
{
    const Table table;

    table.DragTo(2, table.Width(2) + 120);

    const int widened = table.Width(2);
    const int neighbourBefore = table.Width(kSlack);

    table.DragTo(2, widened - 60);

    QCOMPARE(table.Width(2), widened - 60);
    QCOMPARE(table.Width(kSlack), neighbourBefore + 60);
    QCOMPARE(table.TotalWidth(), table.view.viewport()->width());
}

void TableColumnsTest::EveryColumnButTheLastCanBeDragged()
{
    const Table table;

    for (int column = 0; column < kColumns - 1; ++column)
    {
        const int wanted = table.Width(column) + 40;
        table.DragTo(column, wanted);

        QCOMPARE(table.Width(column), wanted);
    }

    QCOMPARE(table.TotalWidth(), table.view.viewport()->width());
}

void TableColumnsTest::TheColumnThatTakesTheSlackIsDraggedLikeAnyOther()
{
    QStandardItemModel model(6, kColumns);
    QTableView view;

    for (int row = 0; row < model.rowCount(); ++row)
    {
        for (int column = 0; column < kColumns; ++column)
        {
            model.setItem(row, column, new QStandardItem(QStringLiteral("cell %1").arg(column)));
        }
    }

    view.setModel(&model);
    view.resize(800, 300);
    LetTheColumnsBeDraggedAndStillFillTheTable(&view, 0);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QHeaderView* header = view.horizontalHeader();
    const int wanted = header->sectionSize(0) - 100;

    header->resizeSection(0, wanted);

    QCOMPARE(header->sectionSize(0), wanted);

    int total = 0;
    for (int column = 0; column < kColumns; ++column)
    {
        total += header->sectionSize(column);
    }

    QCOMPARE(total, view.viewport()->width());
}

void TableColumnsTest::ADraggedColumnKeepsItsLeftEdgeAndMovesTheOneAfterIt()
{
    const Table table;
    const int first = table.Width(0);
    const int second = table.Width(1);
    const int dragged = table.Width(2);
    const int after = table.Width(3);

    QVERIFY(after - 70 > 80);

    table.DragTo(2, dragged + 70);

    QCOMPARE(table.Width(0), first);
    QCOMPARE(table.Width(1), second);
    QCOMPARE(table.Width(2), dragged + 70);
    QCOMPARE(table.Width(3), after - 70);
    QCOMPARE(table.TotalWidth(), table.view.viewport()->width());
}

void TableColumnsTest::TheLastColumnRefusesTheUserBecauseItHasNothingToTradeWith()
{
    const Table table;

    QCOMPARE(table.Header()->sectionResizeMode(kColumns - 1), QHeaderView::Fixed);

    for (int column = 0; column < kColumns - 1; ++column)
    {
        QCOMPARE(table.Header()->sectionResizeMode(column), QHeaderView::Interactive);
    }
}

void TableColumnsTest::AWiderWindowGoesToTheColumnThatTakesTheSlack()
{
    Table table;
    const int narrow = table.Width(0);
    const int slackBefore = table.Width(kSlack);

    table.view.resize(1200, 300);
    static_cast<void>(QTest::qWaitFor(
        [&table, slackBefore]
        {
            return table.Width(kSlack) != slackBefore;
        },
        1000));

    QCOMPARE(table.Width(0), narrow);
    QVERIFY(table.Width(kSlack) > slackBefore);
    QCOMPARE(table.TotalWidth(), table.view.viewport()->width());
}

void TableColumnsTest::ContentThatArrivesAfterTheHelperIsMeasuredJustTheSame()
{
    QStandardItemModel model(0, kColumns);
    QTableView view;

    view.setModel(&model);
    view.resize(800, 300);
    LetTheColumnsBeDraggedAndStillFillTheTable(&view);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    const int narrowBefore = view.horizontalHeader()->sectionSize(0);

    for (int row = 0; row < 6; ++row)
    {
        for (int column = 0; column < kColumns; ++column)
        {
            model.setItem(row, column,
                          new QStandardItem(column == 0 ? QStringLiteral("tfdidesign-aircraft-md-11fge-md-11fpw-pack")
                                                        : QStringLiteral("curto")));
        }
    }

    QVERIFY(QTest::qWaitFor(
        [&view, narrowBefore]
        {
            return view.horizontalHeader()->sectionSize(0) != narrowBefore;
        },
        1000));

    QVERIFY(view.horizontalHeader()->sectionSize(0) > view.horizontalHeader()->sectionSize(1));
    QCOMPARE(view.horizontalHeader()->sectionSize(kSlack),
             view.viewport()->width() - view.horizontalHeader()->sectionSize(0)
                 - view.horizontalHeader()->sectionSize(1) - view.horizontalHeader()->sectionSize(2));
}

void TableColumnsTest::ContentThatChangesWithoutNewRowsIsMeasuredAgain()
{
    const Table table;
    const int narrowBefore = table.Width(0);

    for (int row = 0; row < table.model.rowCount(); ++row)
    {
        table.model.item(row, 0)->setText(QStringLiteral("tfdidesign-md11f"));
    }

    QVERIFY(QTest::qWaitFor(
        [&table, narrowBefore]
        {
            return table.Width(0) != narrowBefore;
        },
        1000));

    QVERIFY(table.Width(0) > narrowBefore);
    QCOMPARE(table.TotalWidth(), table.view.viewport()->width());
}

void TableColumnsTest::TheSlackCanBeGivenToAChosenColumnInsteadOfTheLast()
{
    QStandardItemModel model(6, kColumns);
    QTableView view;

    for (int row = 0; row < model.rowCount(); ++row)
    {
        for (int column = 0; column < kColumns; ++column)
        {
            model.setItem(row, column, new QStandardItem(QStringLiteral("cell %1").arg(column)));
        }
    }

    view.setModel(&model);
    view.resize(800, 300);
    LetTheColumnsBeDraggedAndStillFillTheTable(&view, 0);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    const QHeaderView* header = view.horizontalHeader();
    int total = 0;
    for (int column = 0; column < kColumns; ++column)
    {
        total += header->sectionSize(column);
    }

    QCOMPARE(total, view.viewport()->width());
    QVERIFY(header->sectionSize(0) > header->sectionSize(kColumns - 1));
}

void TableColumnsTest::NoColumnIsSqueezedNarrowerThanItsOwnTitle()
{
    QStandardItemModel model(6, kColumns);
    QTableView view;

    model.setHorizontalHeaderLabels({QStringLiteral("Preset"), QStringLiteral("Conteúdo muito comprido"),
                                     QStringLiteral("Changed"), QStringLiteral("Sobra")});

    for (int row = 0; row < model.rowCount(); ++row)
    {
        for (int column = 0; column < kColumns; ++column)
        {
            model.setItem(row, column, new QStandardItem(QStringLiteral("x")));
        }
    }

    view.setModel(&model);
    view.resize(900, 300);
    LetTheColumnsBeDraggedAndStillFillTheTable(&view);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QHeaderView* header = view.horizontalHeader();

    for (int column = 0; column < kColumns - 1; ++column)
    {
        const int title = header->sectionSizeHint(column);
        QVERIFY(title > 0);

        header->resizeSection(column, 20);

        QVERIFY2(header->sectionSize(column) >= title,
                 qPrintable(QStringLiteral("column %1 ended up with %2, the heading asks for %3")
                                .arg(column)
                                .arg(header->sectionSize(column))
                                .arg(title)));
    }
}

QTEST_MAIN(TableColumnsTest)

#include "tst_table_columns.moc"
