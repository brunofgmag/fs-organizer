#include <QtTest/QtTest>

#include <QtGui/QHelpEvent>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableView>

#include "view/delegates/RowDelegate.h"

class RowDelegateTest : public QObject
{
    Q_OBJECT

private slots:
    static void ATextTooWideForItsColumnAnswersWithATooltip();
    static void ATextThatFitsItsColumnIsLeftWithoutATooltip();
    static void ATooltipTheModelSuppliesWinsOverTheOneMeasuredFromTheColumn();
};

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

QTEST_MAIN(RowDelegateTest)

#include "tst_row_delegate.moc"
