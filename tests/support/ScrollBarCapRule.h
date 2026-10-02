#ifndef FS_ORGANIZER_TESTS_SUPPORT_SCROLL_BAR_CAP_RULE_H
#define FS_ORGANIZER_TESTS_SUPPORT_SCROLL_BAR_CAP_RULE_H

#include <QtTest/QtTest>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QWidget>

#include "tests/support/PhysicalRows.h"

inline void TheScrollBarIsCapped(const QAbstractScrollArea* table)
{
    QVERIFY2(table != nullptr, "the table is not on the screen");
    QVERIFY2(ScrollBarCapOf(*table) != nullptr,
             "without a cap the vertical scroll bar rides over the column header of the table");
}

inline void TheCapRidesWithTheScrollBar(QAbstractScrollArea* table, const QHeaderView* header)
{
    TheScrollBarIsCapped(table);
    QVERIFY2(table->isVisible(), "the rule is about a table the person can see");

    const QWidget* cap = ScrollBarCapOf(*table);
    const int shorterThanOneRow = header->height() + 12;

    if (auto* rows = qobject_cast<QAbstractItemView*>(table); rows != nullptr)
    {
        rows->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    }

    table->setFixedHeight(shorterThanOneRow);
    QCoreApplication::processEvents();

    QVERIFY2(table->verticalScrollBar()->isVisible(),
             qPrintable(QStringLiteral("%1: table %2 px, viewport %3 px, bar range %4..%5, header %6 px")
                            .arg(table->objectName())
                            .arg(table->height())
                            .arg(table->viewport()->height())
                            .arg(table->verticalScrollBar()->minimum())
                            .arg(table->verticalScrollBar()->maximum())
                            .arg(header->height())));
    QVERIFY(cap->isVisible());
    QCOMPARE(cap->height(), header->height());

    table->setFixedHeight(4000);
    QCoreApplication::processEvents();

    QVERIFY(!table->verticalScrollBar()->isVisible());
    QVERIFY(!cap->isVisible());
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_SCROLL_BAR_CAP_RULE_H
