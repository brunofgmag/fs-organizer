#include <QtTest/QtTest>

#include "viewmodel/QuarantineModel.h"

class QuarantineModelTest : public QObject
{
    Q_OBJECT

private slots:
    static void EachQuarantinedFolderIsARowThatSaysWhereItWouldGoBackTo();
    static void AnItemWithoutAnOriginSaysSoInsteadOfShowingAnEmptyCell();
    static void EveryCellOffersItsWholeTextToWhoeverHoversIt();
};

namespace
{
    std::vector<QuarantinedItem> TwoItems()
    {
        return {
            QuarantinedItem{
                "E:/Sim/_fsorganizer-quarantine/simbridge", "E:/Sim/Community/simbridge",
                std::chrono::system_clock::time_point{std::chrono::seconds{1'769'000'000}}
            },
            QuarantinedItem{"D:/Library/_fsorganizer-quarantine/orphan", {}, std::nullopt},
        };
    }
}

void QuarantineModelTest::EachQuarantinedFolderIsARowThatSaysWhereItWouldGoBackTo()
{
    QuarantineModel model;
    model.ShowItems(TwoItems());

    QCOMPARE(model.rowCount({}), 2);
    QCOMPARE(model.data(model.index(0, QuarantineModel::NameColumn), Qt::DisplayRole).toString(),
             QStringLiteral("simbridge"));
    QCOMPARE(model.data(model.index(0, QuarantineModel::OriginColumn), Qt::DisplayRole).toString(),
             QStringLiteral("E:/Sim/Community/simbridge"));
    QCOMPARE(model.data(model.index(0, QuarantineModel::WhereColumn), Qt::DisplayRole).toString(),
             QStringLiteral("E:/Sim/_fsorganizer-quarantine"));
    QVERIFY(!model.data(model.index(0, QuarantineModel::WhenColumn), Qt::DisplayRole).toString()
             .isEmpty());

    QVERIFY(model.ItemAt(model.index(1, 0)) != nullptr);
    QCOMPARE(model.Items().size(), std::size_t{2});
}

void QuarantineModelTest::AnItemWithoutAnOriginSaysSoInsteadOfShowingAnEmptyCell()
{
    QuarantineModel model;
    model.ShowItems(TwoItems());

    QCOMPARE(model.data(model.index(1, QuarantineModel::OriginColumn), Qt::DisplayRole).toString(),
             QStringLiteral("(o diário não sabe)"));
    QCOMPARE(model.data(model.index(1, QuarantineModel::WhenColumn), Qt::DisplayRole).toString(),
             QString());
}

void QuarantineModelTest::EveryCellOffersItsWholeTextToWhoeverHoversIt()
{
    QuarantineModel model;
    model.ShowItems(TwoItems());

    for (int column = 0; column <= QuarantineModel::WhereColumn; ++column)
    {
        const QModelIndex cell = model.index(0, column);

        QCOMPARE(model.data(cell, Qt::ToolTipRole).toString(),
                 model.data(cell, Qt::DisplayRole).toString());
    }

    QCOMPARE(model.data(model.index(0, QuarantineModel::OriginColumn), Qt::ToolTipRole).toString(),
             QStringLiteral("E:/Sim/Community/simbridge"));
}

QTEST_APPLESS_MAIN(QuarantineModelTest)

#include "tst_quarantine_model.moc"
