#ifndef FS_ORGANIZER_TESTS_SUPPORT_FIXED_HEADINGS_H
#define FS_ORGANIZER_TESTS_SUPPORT_FIXED_HEADINGS_H

#include <vector>

#include <QtTest/QtTest>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QScrollBar>

[[nodiscard]] inline QScrollArea* ListOf(const QDialog& dialog)
{
    return dialog.findChild<QScrollArea*>();
}

[[nodiscard]] inline QWidget* HeadingsOf(const QDialog& dialog)
{
    return dialog.findChild<QWidget*>(QStringLiteral("ColumnHeadings"));
}

[[nodiscard]] inline QList<QWidget*> RulesOf(const QDialog& dialog)
{
    return dialog.findChildren<QWidget*>(QStringLiteral("ListRule"));
}

[[nodiscard]] inline QStringList HeadingTextsOf(const QDialog& dialog)
{
    QStringList said;

    for (const QLabel* label : HeadingsOf(dialog)->findChildren<QLabel*>())
    {
        said.append(label->text());
    }

    return said;
}

[[nodiscard]] inline std::vector<QWidget*> FirstRowOf(const QDialog& dialog)
{
    const auto* grid = qobject_cast<QGridLayout*>(ListOf(dialog)->widget()->layout());
    std::vector<QWidget*> cells;
    cells.reserve(static_cast<std::size_t>(grid->columnCount()));

    for (int column = 0; column < grid->columnCount(); ++column)
    {
        cells.push_back(grid->itemAtPosition(0, column)->widget());
    }

    return cells;
}

[[nodiscard]] inline QPoint PlaceIn(const QDialog& dialog, const QWidget& widget)
{
    return widget.mapTo(&dialog, QPoint());
}

inline void ShowItSettled(QDialog& dialog)
{
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QCoreApplication::processEvents();
}

inline void VerifyTheHeadingsAreOutsideTheScroll(const QDialog& dialog, const QStringList& named)
{
    const QScrollArea* list = ListOf(dialog);
    const QWidget* headings = HeadingsOf(dialog);

    QVERIFY(headings != nullptr);
    QVERIFY2(!list->widget()->isAncestorOf(headings), "the headings sit inside what scrolls");
    QVERIFY2(!list->isAncestorOf(headings), "the headings sit inside the scroll area");
    QCOMPARE(HeadingTextsOf(dialog), named);
}

inline void VerifyEachHeadingStandsOverItsColumn(const QDialog& dialog)
{
    const QWidget* headings = HeadingsOf(dialog);
    const std::vector<QWidget*> cells = FirstRowOf(dialog);
    const QList<QLabel*> labels = headings->findChildren<QLabel*>();

    QCOMPARE(static_cast<std::size_t>(labels.size()), cells.size());

    for (std::size_t column = 0; column < cells.size(); ++column)
    {
        QVERIFY2(PlaceIn(dialog, *labels.at(static_cast<qsizetype>(column))).x() == PlaceIn(dialog, *cells[column]).x(),
                 qPrintable(QStringLiteral("the heading \"%1\" is %2 px from the column below it")
                                .arg(labels.at(static_cast<qsizetype>(column))->text())
                                .arg(PlaceIn(dialog, *labels.at(static_cast<qsizetype>(column))).x()
                                     - PlaceIn(dialog, *cells[column]).x())));
        QCOMPARE(labels.at(static_cast<qsizetype>(column))->width(), cells[column]->width());
    }
}

inline void VerifyTheListScrollsForRealAndTheHeadingsStay(const QDialog& dialog)
{
    QScrollBar* bar = ListOf(dialog)->verticalScrollBar();
    QVERIFY2(bar->isVisible() && bar->maximum() > 0, "the list is meant to be taller than its room");

    const std::vector<QWidget*> cells = FirstRowOf(dialog);
    const int rowBefore = PlaceIn(dialog, *cells.front()).y();
    const int headingsBefore = PlaceIn(dialog, *HeadingsOf(dialog)).y();

    bar->setValue(bar->maximum());
    QCoreApplication::processEvents();

    QVERIFY2(PlaceIn(dialog, *cells.front()).y() < rowBefore, "the list did not move: nothing really scrolled");
    QCOMPARE(PlaceIn(dialog, *HeadingsOf(dialog)).y(), headingsBefore);

    VerifyEachHeadingStandsOverItsColumn(dialog);
}

inline void VerifyTwoRulesFrameTheList(const QDialog& dialog)
{
    const QList<QWidget*> rules = RulesOf(dialog);
    QCOMPARE(rules.size(), qsizetype{2});

    const QWidget* headings = HeadingsOf(dialog);
    const QScrollArea* list = ListOf(dialog);

    QCOMPARE(list->frameShape(), QFrame::NoFrame);

    const QWidget* upper =
        PlaceIn(dialog, *rules.front()).y() < PlaceIn(dialog, *rules.back()).y() ? rules.front() : rules.back();
    const QWidget* lower = upper == rules.front() ? rules.back() : rules.front();

    QVERIFY(PlaceIn(dialog, *upper).y() >= PlaceIn(dialog, *headings).y() + headings->height());
    QVERIFY(PlaceIn(dialog, *upper).y() + upper->height() <= PlaceIn(dialog, *list).y());
    QVERIFY(PlaceIn(dialog, *lower).y() >= PlaceIn(dialog, *list).y() + list->height());
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_FIXED_HEADINGS_H
