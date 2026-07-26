#include <QtTest/QtTest>

#include <QtGui/QStandardItemModel>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableView>

#include "view/TableColumns.h"

class TableColumnsTest : public QObject
{
    Q_OBJECT

private slots:
    static void TheColumnsFillTheTableWithoutLeavingAStripOnTheRight();
    static void WideningOneColumnWidensThatColumnAndNoOther();
    static void NarrowingOneColumnIsAbsorbedInsteadOfLeavingAGap();
    static void EveryColumnButTheOneThatTakesTheSlackCanBeDragged();
    static void AWiderWindowGoesToTheColumnThatTakesTheSlack();
};

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
    const int slackBefore = table.Width(kSlack);

    table.DragTo(1, table.Width(1) - 60);

    QCOMPARE(table.Width(kSlack), slackBefore + 60);
    QCOMPARE(table.TotalWidth(), table.view.viewport()->width());
}

void TableColumnsTest::EveryColumnButTheOneThatTakesTheSlackCanBeDragged()
{
    const Table table;

    for (int column = 0; column < kSlack; ++column)
    {
        const int wanted = table.Width(column) + 40;
        table.DragTo(column, wanted);

        QCOMPARE(table.Width(column), wanted);
    }

    QCOMPARE(table.TotalWidth(), table.view.viewport()->width());
}

void TableColumnsTest::AWiderWindowGoesToTheColumnThatTakesTheSlack()
{
    Table table;
    const int narrow = table.Width(0);
    const int slackBefore = table.Width(kSlack);

    table.view.resize(1200, 300);
    static_cast<void>(QTest::qWaitFor([&table, slackBefore]
    {
        return table.Width(kSlack) != slackBefore;
    }, 1000));

    QCOMPARE(table.Width(0), narrow);
    QVERIFY(table.Width(kSlack) > slackBefore);
    QCOMPARE(table.TotalWidth(), table.view.viewport()->width());
}

QTEST_MAIN(TableColumnsTest)

#include "tst_table_columns.moc"
