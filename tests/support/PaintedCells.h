#ifndef FS_ORGANIZER_TESTS_SUPPORT_PAINTED_CELLS_H
#define FS_ORGANIZER_TESTS_SUPPORT_PAINTED_CELLS_H

#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <QtTest/QtTest>
#include <QtWidgets/QAbstractItemView>

inline constexpr int kPixelsInsideTheCell = 3;

inline QImage TheViewportAsPainted(const QAbstractItemView& view)
{
    QPixmap shot(view.viewport()->size());
    shot.fill(Qt::transparent);
    view.viewport()->render(&shot);

    return shot.toImage();
}

inline int WhereTheInkEnds(const QImage& painted, const QRect& cell)
{
    const QColor ground = painted.pixelColor(cell.right() - 1, cell.center().y());

    for (int x = cell.right(); x >= cell.left(); --x)
    {
        for (int y = cell.top(); y <= cell.bottom(); ++y)
        {
            if (painted.pixelColor(x, y) != ground)
            {
                return x;
            }
        }
    }

    return -1;
}

inline void TheSelectedRowRunsThroughTheLastColumn(const QAbstractItemView& view, const int row)
{
    const int last = view.model()->columnCount() - 1;
    const QRect cell = view.visualRect(view.model()->index(row, last));

    QVERIFY(cell.width() > 2 * kPixelsInsideTheCell);

    const QImage painted = TheViewportAsPainted(view);
    const int middle = cell.center().y();

    const QColor onTheDivide = painted.pixelColor(cell.left(), middle);
    const QColor inside = painted.pixelColor(cell.left() + kPixelsInsideTheCell, middle);
    const QColor atTheEnd = painted.pixelColor(cell.right(), middle);

    QVERIFY2(onTheDivide == inside,
             qPrintable(QStringLiteral("a line cuts the selected row at the divide before the last column: %1 "
                                       "against %2")
                            .arg(onTheDivide.name(), inside.name())));
    QVERIFY2(atTheEnd != inside, "the selected row lost its right edge");
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_PAINTED_CELLS_H
