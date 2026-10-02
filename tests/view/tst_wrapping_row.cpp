#include <algorithm>

#include <QtTest/QtTest>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLayoutItem>
#include <QtWidgets/QWidget>

#include "view/WrappingRow.h"

namespace
{
    class WrappingRowTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheSizeHintLeavesOutTheSpringThatOnlyTheLowerLineUses();
        static void TheFourActionsStayOnTheFirstLineAndTheRestStepsDown();
        static void TheLastItemEndsAtTheRightMarginAtEveryWidthFromTwoLinesUp();
        static void OnOneLineTheChipsSitThirtyTwoPixelsFromTheLastActionAndTheLowerSpringIsEmpty();
        static void AtTheNarrowestTwoLineWidthTheCheckboxSitsFortyPixelsFromTheLastChip();
        static void TheHeightForWidthIsTheBottomOfWhatWasPlacedAtEveryWidth();
        static void TakingAnItemOutClearsWhatTheRowKnewAboutIt();
    };
}

namespace
{
    constexpr int kMargin = 10;
    constexpr int kGap = 8;
    constexpr int kLineHeight = 28;
    constexpr int kActionWidth = 90;
    constexpr int kChipWidth = 70;
    constexpr int kChipGap = 6;
    constexpr int kTrailingMargin = 8;
    constexpr int kCheckboxWidth = 150;
    constexpr int kSearchMinimum = 120;
    constexpr int kSearchMaximum = 220;
    constexpr int kLeastSpring = 16;
    constexpr int kFirstWidth = 400;
    constexpr int kLastWidth = 2000;

    QWidget* Plain(const int width, const int height, QWidget* parent)
    {
        auto* widget = new QWidget(parent);
        widget->setFixedSize(width, height);

        return widget;
    }

    struct Bar
    {
        Bar()
        {
            row = new WrappingRow(&host);
            row->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
            row->setSpacing(kGap);

            for (QWidget*& action : actions)
            {
                action = Plain(kActionWidth, kLineHeight, &host);
                row->addWidget(action);
            }

            row->AddSpring();

            filter = new QWidget(&host);
            filter->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            auto* chipLine = new QHBoxLayout(filter);
            chipLine->setContentsMargins(0, 0, kTrailingMargin, 0);
            chipLine->setSpacing(kChipGap);

            for (QWidget*& chip : chips)
            {
                chip = Plain(kChipWidth, kLineHeight, filter);
                chipLine->addWidget(chip);
            }

            row->AddWidgetThatStepsDown(filter);
            row->AddSpringOnTheLowerLine();

            checkbox = Plain(kCheckboxWidth, kLineHeight, &host);
            row->AddWidgetThatStepsDown(checkbox);

            search = new QWidget(&host);
            search->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            search->setMinimumSize(kSearchMinimum, kLineHeight);
            search->setMaximumSize(kSearchMaximum, kLineHeight);
            row->AddWidgetThatStepsDown(search);

            host.setAttribute(Qt::WA_DontShowOnScreen);
            host.show();
        }

        void Lay(const int width) const
        {
            row->setGeometry(QRect(0, 0, width, row->heightForWidth(width)));
            filter->layout()->activate();
            filter->layout()->setGeometry(QRect(QPoint(0, 0), filter->geometry().size()));
        }

        [[nodiscard]] int OneLineFrom() const
        {
            return row->sizeHint().width();
        }

        [[nodiscard]] int TwoLinesHeight() const
        {
            return row->heightForWidth(OneLineFrom() - 1);
        }

        [[nodiscard]] int TwoLinesFrom() const
        {
            for (int width = kFirstWidth; width < OneLineFrom(); ++width)
            {
                if (row->heightForWidth(width) == TwoLinesHeight())
                {
                    return width;
                }
            }

            return OneLineFrom();
        }

        [[nodiscard]] QRect PlaceOf(const QWidget* widget) const
        {
            return widget->geometry();
        }

        [[nodiscard]] QRect LastChipPlace() const
        {
            return QRect(filter->geometry().topLeft() + chips[2]->geometry().topLeft(), chips[2]->size());
        }

        QWidget host;
        WrappingRow* row = nullptr;
        QWidget* actions[4] = {};
        QWidget* filter = nullptr;
        QWidget* chips[3] = {};
        QWidget* checkbox = nullptr;
        QWidget* search = nullptr;
    };

    int Right(const QRect& place)
    {
        return place.x() + place.width();
    }

    int Bottom(const QRect& place)
    {
        return place.y() + place.height();
    }
}

void WrappingRowTest::TheSizeHintLeavesOutTheSpringThatOnlyTheLowerLineUses()
{
    const Bar bar;

    const int chipsWidth = 3 * kChipWidth + 2 * kChipGap + kTrailingMargin;
    const int widthsOnOneLine = 4 * kActionWidth + kLeastSpring + chipsWidth + kCheckboxWidth + kSearchMinimum;
    const int gapsOnOneLine = 7 * kGap;

    QCOMPARE(bar.row->count(), 9);
    QCOMPARE(bar.row->sizeHint().width(), widthsOnOneLine + gapsOnOneLine + 2 * kMargin);
    QCOMPARE(bar.row->sizeHint().height(), kLineHeight + 2 * kMargin);
}

void WrappingRowTest::TheFourActionsStayOnTheFirstLineAndTheRestStepsDown()
{
    const Bar bar;
    bar.Lay(bar.TwoLinesFrom() + 37);

    const int firstLine = kMargin;
    const int secondLine = kMargin + kLineHeight + kGap;

    for (const QWidget* action : bar.actions)
    {
        QCOMPARE(bar.PlaceOf(action).y(), firstLine);
    }

    QCOMPARE(bar.PlaceOf(bar.actions[0]).x(), kMargin);
    QCOMPARE(bar.PlaceOf(bar.filter).y(), secondLine);
    QCOMPARE(bar.PlaceOf(bar.filter).x(), kMargin);
    QCOMPARE(bar.PlaceOf(bar.checkbox).y(), secondLine);
    QCOMPARE(bar.PlaceOf(bar.search).y(), secondLine);
    QVERIFY(bar.PlaceOf(bar.checkbox).x() > Right(bar.PlaceOf(bar.filter)));
    QCOMPARE(bar.row->heightForWidth(bar.TwoLinesFrom() + 37), 2 * kLineHeight + kGap + 2 * kMargin);
}

void WrappingRowTest::TheLastItemEndsAtTheRightMarginAtEveryWidthFromTwoLinesUp()
{
    const Bar bar;
    const int from = bar.TwoLinesFrom();

    QVERIFY(from < bar.OneLineFrom());

    for (int width = from; width <= kLastWidth; ++width)
    {
        bar.Lay(width);

        QVERIFY2(Right(bar.PlaceOf(bar.search)) == width - kMargin,
                 qPrintable(QStringLiteral("at %1 px the search ends at %2, %3 from the margin")
                                .arg(width)
                                .arg(Right(bar.PlaceOf(bar.search)))
                                .arg(width - kMargin - Right(bar.PlaceOf(bar.search)))));
        QVERIFY(bar.search->width() >= kSearchMinimum);
        QVERIFY(bar.search->width() <= kSearchMaximum);
    }
}

void WrappingRowTest::OnOneLineTheChipsSitThirtyTwoPixelsFromTheLastActionAndTheLowerSpringIsEmpty()
{
    const Bar bar;

    for (const int width : {bar.OneLineFrom(), bar.OneLineFrom() + 1, kLastWidth})
    {
        bar.Lay(width);

        QCOMPARE(bar.row->heightForWidth(width), kLineHeight + 2 * kMargin);
        QCOMPARE(bar.PlaceOf(bar.filter).y(), kMargin);
        QVERIFY(bar.PlaceOf(bar.filter).x() - Right(bar.PlaceOf(bar.actions[3])) >= kGap + kLeastSpring + kGap);
        QVERIFY(bar.row->itemAt(6)->geometry().isEmpty());
    }

    bar.Lay(bar.OneLineFrom());

    QCOMPARE(bar.PlaceOf(bar.filter).x() - Right(bar.PlaceOf(bar.actions[3])), 2 * kGap + kLeastSpring);
    QCOMPARE(bar.PlaceOf(bar.filter).x() - Right(bar.PlaceOf(bar.actions[3])), 32);
}

void WrappingRowTest::AtTheNarrowestTwoLineWidthTheCheckboxSitsFortyPixelsFromTheLastChip()
{
    const Bar bar;
    bar.Lay(bar.TwoLinesFrom());

    QCOMPARE(bar.row->heightForWidth(bar.TwoLinesFrom()), bar.TwoLinesHeight());
    QCOMPARE(bar.row->heightForWidth(bar.TwoLinesFrom() - 1), 3 * kLineHeight + 2 * kGap + 2 * kMargin);
    QCOMPARE(bar.PlaceOf(bar.checkbox).x() - Right(bar.LastChipPlace()), 40);
    QCOMPARE(bar.PlaceOf(bar.search).width(), kSearchMinimum);
    QCOMPARE(Right(bar.PlaceOf(bar.search)), bar.TwoLinesFrom() - kMargin);
    QCOMPARE(bar.PlaceOf(bar.search).x() - Right(bar.PlaceOf(bar.checkbox)), kGap);
}

void WrappingRowTest::TheHeightForWidthIsTheBottomOfWhatWasPlacedAtEveryWidth()
{
    const Bar bar;

    for (int width = kFirstWidth; width <= kLastWidth; ++width)
    {
        const int promised = bar.row->heightForWidth(width);
        bar.Lay(width);

        int lowest = 0;
        int rightmost = 0;

        for (int at = 0; at < bar.row->count(); ++at)
        {
            if (const QRect place = bar.row->itemAt(at)->geometry(); !place.isEmpty())
            {
                lowest = std::max(lowest, Bottom(place));
                rightmost = std::max(rightmost, Right(place));
            }
        }

        QVERIFY2(lowest + kMargin == promised,
                 qPrintable(QStringLiteral("at %1 px the row promised %2 and placed down to %3")
                                .arg(width)
                                .arg(promised)
                                .arg(lowest + kMargin)));
        QVERIFY2(rightmost + kMargin <= width,
                 qPrintable(QStringLiteral("at %1 px something reaches %2").arg(width).arg(rightmost + kMargin)));
    }
}

void WrappingRowTest::TakingAnItemOutClearsWhatTheRowKnewAboutIt()
{
    const Bar bar;
    const int before = bar.row->sizeHint().width();

    QLayoutItem* lowerSpring = bar.row->takeAt(6);

    QVERIFY(lowerSpring != nullptr);
    QCOMPARE(bar.row->count(), 8);
    QCOMPARE(bar.row->sizeHint().width(), before);

    bar.row->addItem(lowerSpring);

    QCOMPARE(bar.row->count(), 9);
    QCOMPARE(bar.row->sizeHint().width(), before + kGap + kLeastSpring);
    QVERIFY(bar.row->takeAt(42) == nullptr);
}

QTEST_MAIN(WrappingRowTest)

#include "tst_wrapping_row.moc"
