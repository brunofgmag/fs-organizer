#include <QtTest/QtTest>

#include "viewmodel/QuarantineModel.h"

class QuarantineModelTest : public QObject
{
    Q_OBJECT

private slots:
    static void EachQuarantinedFolderIsARowThatSaysWhereItWouldGoBackTo();
    static void AnItemTheJournalNeverSawSaysSoInBothColumnsInsteadOfShowingAnEmptyCell();
    static void NoCellRepeatsItsOwnTextAsATooltip();
};

namespace
{
    std::vector<QuarantinedItem> TwoItems()
    {
        return {
            QuarantinedItem{"E:/Sim/_fsorganizer-quarantine/simbridge", "E:/Sim/Community/simbridge",
                            std::chrono::system_clock::time_point{std::chrono::seconds{1'769'000'000}}},
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
             QStringLiteral(R"(E:\Sim\Community\simbridge)"));
    QCOMPARE(model.data(model.index(0, QuarantineModel::WhereColumn), Qt::DisplayRole).toString(),
             QStringLiteral(R"(E:\Sim\_fsorganizer-quarantine)"));
    QVERIFY(!model.data(model.index(0, QuarantineModel::WhenColumn), Qt::DisplayRole).toString().isEmpty());

    QVERIFY(model.ItemAt(model.index(1, 0)) != nullptr);
    QCOMPARE(model.Items().size(), std::size_t{2});
}

void QuarantineModelTest::AnItemTheJournalNeverSawSaysSoInBothColumnsInsteadOfShowingAnEmptyCell()
{
    QuarantineModel model;
    model.ShowItems(TwoItems());

    QCOMPARE(model.data(model.index(1, QuarantineModel::OriginColumn), Qt::DisplayRole).toString(),
             QStringLiteral("(o diário não sabe)"));
    QCOMPARE(model.data(model.index(1, QuarantineModel::WhenColumn), Qt::DisplayRole).toString(),
             QStringLiteral("(o diário não sabe)"));
}

void QuarantineModelTest::NoCellRepeatsItsOwnTextAsATooltip()
{
    QuarantineModel model;
    model.ShowItems(TwoItems());

    for (int column = 0; column <= QuarantineModel::WhereColumn; ++column)
    {
        QVERIFY(model.data(model.index(0, column), Qt::ToolTipRole).toString().isEmpty());
    }
}

QTEST_APPLESS_MAIN(QuarantineModelTest)

#include "tst_quarantine_model.moc"
