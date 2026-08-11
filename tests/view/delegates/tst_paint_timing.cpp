#include <QtTest/QtTest>

#include <QtGui/QPixmap>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableView>

#include "view/delegates/RowDelegate.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    class PaintTimingTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void PaintingTheSameRowsAgainAsksTheFontNothingNew();
        static void RowsCarryingTheSameTextAskTheFontOnceInsteadOfOncePerRow();
        static void ARowWithNothingToLayAfterTheTextIsNeverMeasuredForAnAdvance();
    };

    constexpr int kRows = 20;
    constexpr auto kName = "asobo-aircraft-c172sp-classic";

    struct Painted
    {
        QStandardItemModel model{kRows, 1};
        QTableView view;
        RowDelegate delegate;

        Painted()
        {
            view.setModel(&model);
            view.setItemDelegate(&delegate);
            view.verticalHeader()->setVisible(false);
            view.horizontalHeader()->setVisible(false);
            view.setShowGrid(false);
            view.resize(520, 1400);
            view.horizontalHeader()->resizeSection(0, 260);
        }

        void Carry(const QString& text, const QString& suffix)
        {
            for (int row = 0; row < kRows; ++row)
            {
                auto* cell = new QStandardItem(text);

                if (!suffix.isEmpty())
                {
                    cell->setData(suffix, QuietSuffixRole);
                }

                model.setItem(row, 0, cell);
            }
        }

        void CarryOnePerRow()
        {
            for (int row = 0; row < kRows; ++row)
            {
                model.setItem(row, 0, new QStandardItem(QStringLiteral("%1-%2").arg(kName).arg(row)));
            }
        }

        [[nodiscard]] bool Show()
        {
            view.show();

            return QTest::qWaitForWindowExposed(&view);
        }

        void PaintTheViewport()
        {
            QPixmap shot(view.viewport()->size());
            shot.fill(Qt::transparent);
            view.viewport()->render(&shot);
        }

        [[nodiscard]] int Asks() const
        {
            return delegate.TimesItAskedTheFont();
        }
    };
}

void PaintTimingTest::PaintingTheSameRowsAgainAsksTheFontNothingNew()
{
    Painted list;
    list.CarryOnePerRow();
    QVERIFY(list.Show());

    list.PaintTheViewport();
    const int afterTheFirstFrame = list.Asks();

    list.PaintTheViewport();
    list.PaintTheViewport();

    QVERIFY2(afterTheFirstFrame > 1, "the first frame asked the font almost nothing, so the viewport painted nothing");
    QCOMPARE(list.Asks(), afterTheFirstFrame);
}

void PaintTimingTest::RowsCarryingTheSameTextAskTheFontOnceInsteadOfOncePerRow()
{
    Painted repeated;
    repeated.Carry(QString::fromLatin1(kName), {});
    QVERIFY(repeated.Show());

    Painted distinct;
    distinct.CarryOnePerRow();
    QVERIFY(distinct.Show());

    repeated.PaintTheViewport();
    distinct.PaintTheViewport();

    QCOMPARE(repeated.Asks(), 1);
    QVERIFY2(distinct.Asks() > repeated.Asks(), "both lists asked the same, so the elision is not being remembered");
}

void PaintTimingTest::ARowWithNothingToLayAfterTheTextIsNeverMeasuredForAnAdvance()
{
    Painted bare;
    bare.Carry(QString::fromLatin1(kName), {});
    QVERIFY(bare.Show());

    Painted suffixed;
    suffixed.Carry(QString::fromLatin1(kName), QStringLiteral("linked"));
    QVERIFY(suffixed.Show());

    bare.PaintTheViewport();
    suffixed.PaintTheViewport();

    QCOMPARE(bare.Asks(), 1);
    QVERIFY2(suffixed.Asks() > kRows, "the suffixed rows were never laid out, so the bare count proves nothing");
}

QTEST_MAIN(PaintTimingTest)

#include "tst_paint_timing.moc"
