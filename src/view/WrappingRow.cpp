#include "view/WrappingRow.h"

#include <algorithm>

#include <QtWidgets/QWidget>

namespace
{
    constexpr int kDefaultGap = 8;

    bool ItTakesTheSlack(const QLayoutItem* item)
    {
        return (item->expandingDirections() & Qt::Horizontal) != 0;
    }

    int HowMuchMoreItWillTake(const QLayoutItem* item)
    {
        return std::max(0, item->maximumSize().width() - item->sizeHint().width());
    }

    int TallestIn(const QList<QLayoutItem*>& row)
    {
        int tall = 0;
        for (const QLayoutItem* item : row)
        {
            tall = std::max(tall, item->sizeHint().height());
        }

        return tall;
    }
}

WrappingRow::WrappingRow(QWidget* parent) : QLayout(parent)
{
}

WrappingRow::~WrappingRow()
{
    while (!items_.isEmpty())
    {
        delete items_.takeFirst();
    }
}

void WrappingRow::AddSpring()
{
    addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
}

void WrappingRow::addItem(QLayoutItem* item)
{
    items_.append(item);
}

int WrappingRow::count() const
{
    return static_cast<int>(items_.size());
}

QLayoutItem* WrappingRow::itemAt(const int index) const
{
    return items_.value(index);
}

QLayoutItem* WrappingRow::takeAt(const int index)
{
    return index >= 0 && index < items_.size() ? items_.takeAt(index) : nullptr;
}

Qt::Orientations WrappingRow::expandingDirections() const
{
    return {};
}

bool WrappingRow::hasHeightForWidth() const
{
    return true;
}

int WrappingRow::heightForWidth(const int width) const
{
    const QMargins around = contentsMargins();

    int height = around.top() + around.bottom();
    for (const QList<QLayoutItem*>& row : LinesThatFit(width - around.left() - around.right()))
    {
        height += TallestIn(row) + Gap();
    }

    return height - Gap();
}

void WrappingRow::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);

    const QMargins around = contentsMargins();
    const QRect inside = rect.adjusted(around.left(), around.top(), -around.right(), -around.bottom());

    int y = inside.y();
    for (const QList<QLayoutItem*>& row : LinesThatFit(inside.width()))
    {
        const int tall = TallestIn(row);

        PlaceTheLine(row, QRect(inside.x(), y, inside.width(), tall));

        y += tall + Gap();
    }
}

QSize WrappingRow::sizeHint() const
{
    const QMargins around = contentsMargins();

    int width = 0;
    int height = 0;
    for (const QLayoutItem* item : items_)
    {
        width += item->sizeHint().width() + Gap();
        height = std::max(height, item->sizeHint().height());
    }

    return {width - Gap() + around.left() + around.right(), height + around.top() + around.bottom()};
}

QSize WrappingRow::minimumSize() const
{
    const QMargins around = contentsMargins();

    QSize widest;
    for (const QLayoutItem* item : items_)
    {
        widest = widest.expandedTo(item->minimumSize());
    }

    return widest + QSize(around.left() + around.right(), around.top() + around.bottom());
}

int WrappingRow::Gap() const
{
    return spacing() >= 0 ? spacing() : kDefaultGap;
}

QList<QList<QLayoutItem*>> WrappingRow::LinesThatFit(const int width) const
{
    QList<QList<QLayoutItem*>> lines;
    QList<QLayoutItem*> line;
    int taken = 0;

    for (QLayoutItem* item : items_)
    {
        const int wants = item->sizeHint().width();
        const int wouldBe = line.isEmpty() ? wants : taken + Gap() + wants;

        if (!line.isEmpty() && wouldBe > width)
        {
            lines.append(line);
            line.clear();
            taken = wants;
        }
        else
        {
            taken = wouldBe;
        }

        line.append(item);
    }

    if (!line.isEmpty())
    {
        lines.append(line);
    }

    return lines;
}

void WrappingRow::PlaceTheLine(const QList<QLayoutItem*>& row, const QRect& where) const
{
    int used = Gap() * static_cast<int>(row.size() - 1);
    for (const QLayoutItem* item : row)
    {
        used += item->sizeHint().width();
    }

    int stillToGive = std::max(0, where.width() - used);
    int takers = static_cast<int>(std::ranges::count_if(row, ItTakesTheSlack));

    int x = where.x();
    for (QLayoutItem* item : row)
    {
        int width = item->sizeHint().width();

        if (ItTakesTheSlack(item) && takers > 0)
        {
            const int share = std::min(stillToGive / takers, HowMuchMoreItWillTake(item));
            width += share;
            stillToGive -= share;
            --takers;
        }

        item->setGeometry(QRect(x, where.y(), width, where.height()));
        x += width + Gap();
    }
}
