#ifndef FS_ORGANIZER_TESTS_SUPPORT_PHYSICAL_ROWS_H
#define FS_ORGANIZER_TESTS_SUPPORT_PHYSICAL_ROWS_H

#include <vector>

#include <QtCore/QtMath>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QImage>
#include <QtTest/QtTest>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QWidget>

#include "view/theme/ModernistTones.h"

inline void MakeTheColumnHeaderOnePixelShorter(QHeaderView& header)
{
    header.setMaximumHeight(header.height() - 1);
    QCoreApplication::processEvents();
}

inline QImage PhotographAt(QWidget& page, const qreal ratio)
{
    QImage image((QSizeF(page.size()) * ratio).toSize(), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(ratio);
    image.fill(Qt::magenta);
    page.render(&image);

    return image;
}

inline QString Spelled(const std::vector<int>& rows)
{
    QStringList said;
    for (const int row : rows)
    {
        said << QString::number(row);
    }

    return said.join(QLatin1Char(','));
}

inline std::vector<int> RowsOfTheRuleIn(const QImage& photo, const QWidget& page, const QWidget& inside, const int x)
{
    const qreal ratio = photo.devicePixelRatio();
    const QPoint top = inside.mapTo(&page, QPoint{x, 0});
    const int column = qRound(top.x() * ratio);
    const int from = qRound(top.y() * ratio);
    const int to = qMin(photo.height(), qRound((top.y() + inside.height()) * ratio));
    const QRgb rule = TonesOf(CurrentColorScheme()).divider.rgb();

    std::vector<int> rows;
    for (int row = from; row < to; ++row)
    {
        if (photo.pixelColor(column, row).rgb() == rule)
        {
            rows.push_back(row);
        }
    }

    return rows;
}

inline std::vector<int> ColumnsOfTheRuleIn(const QImage& photo, const QWidget& page, const QWidget& edge, const int y)
{
    const qreal ratio = photo.devicePixelRatio();
    const QPoint left = edge.mapTo(&page, QPoint{0, y});
    const int row = qRound(left.y() * ratio);
    const int from = qMax(0, qRound(left.x() * ratio) - 2);
    const int to = qMin(photo.width(), qRound(left.x() * ratio) + qCeil(ratio) + 2);
    const QRgb rule = TonesOf(CurrentColorScheme()).divider.rgb();

    std::vector<int> columns;
    for (int column = from; column < to; ++column)
    {
        if (photo.pixelColor(column, row).rgb() == rule)
        {
            columns.push_back(column);
        }
    }

    return columns;
}

inline void TheLeftRuleRunsTheWholeHeightOfThePanel(QWidget& page, const QWidget& strip, const QWidget& body)
{
    for (const qreal ratio : {1.0, 1.25, 1.5, 1.75})
    {
        const QImage photo = PhotographAt(page, ratio);
        const std::vector<int> panel = ColumnsOfTheRuleIn(photo, page, strip, strip.height() / 2);

        QVERIFY2(!panel.empty(), qPrintable(QStringLiteral("no left rule on the title strip at %1").arg(ratio)));

        for (const int y : {1, body.height() / 2, body.height() - 2})
        {
            const std::vector<int> below = ColumnsOfTheRuleIn(photo, page, body, y);

            QVERIFY2(below == panel,
                     qPrintable(QStringLiteral("at %1 the title strip left rule is on columns %2 and the body left "
                                               "rule, %3 rows down, on %4")
                                    .arg(ratio)
                                    .arg(Spelled(panel))
                                    .arg(y)
                                    .arg(Spelled(below))));
        }
    }
}

inline QWidget* ScrollBarCapOf(const QAbstractScrollArea& view)
{
    return view.findChild<QWidget*>(QStringLiteral("ScrollBarCap"));
}

inline void LetTheScrollBarShow(QWidget& page, const QAbstractScrollArea& view)
{
    for (int height = 600; height >= 40 && !view.verticalScrollBar()->isVisible(); height -= 20)
    {
        page.resize(page.width(), height);
        QCoreApplication::processEvents();
    }
}

inline void LetTheScrollBarGo(QWidget& page)
{
    page.resize(page.width(), 4000);
    QCoreApplication::processEvents();
}

inline void TheThreeRulesLandOnTheSamePhysicalRows(QWidget& page,
                                                   const QWidget& strip,
                                                   const QHeaderView& header,
                                                   const QWidget& cap)
{
    for (const qreal ratio : {1.0, 1.25, 1.5, 1.75})
    {
        const QImage photo = PhotographAt(page, ratio);
        const std::vector<int> table = RowsOfTheRuleIn(photo, page, header, header.width() - 4);
        const std::vector<int> panel = RowsOfTheRuleIn(photo, page, strip, strip.width() / 2);
        const std::vector<int> scrollBar = RowsOfTheRuleIn(photo, page, cap, cap.width() / 2);

        QVERIFY2(!table.empty() && !panel.empty() && !scrollBar.empty(),
                 qPrintable(QStringLiteral("no rule found at %1").arg(ratio)));
        QVERIFY2(table == panel,
                 qPrintable(QStringLiteral("at %1 the column header rule is on rows %2 and the title strip rule on %3")
                                .arg(ratio)
                                .arg(Spelled(table), Spelled(panel))));
        QVERIFY2(table == scrollBar,
                 qPrintable(QStringLiteral("at %1 the column header rule is on rows %2 and the scroll bar cap rule "
                                           "on %3")
                                .arg(ratio)
                                .arg(Spelled(table), Spelled(scrollBar))));
    }
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_PHYSICAL_ROWS_H
