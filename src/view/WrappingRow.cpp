#include "view/WrappingRow.h"

#include <algorithm>
#include <functional>
#include <iterator>

#include <QtCore/QHash>

#include <QtWidgets/QWidget>

#include "view/theme/ModernistMetrics.h"

namespace
{
    constexpr int kDefaultGap = 8;

    QSpacerItem* NewSpring()
    {
        return new QSpacerItem(kSpringAtLeast, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    }

    bool ItTakesTheSlack(const QLayoutItem* item)
    {
        return (item->expandingDirections() & Qt::Horizontal) != 0;
    }

    int HowMuchMoreItWillTake(const QLayoutItem* item)
    {
        return std::max(0, item->maximumSize().width() - item->sizeHint().width());
    }

    bool ItIsHidden(const QLayoutItem* item)
    {
        return item->widget() != nullptr && item->isEmpty();
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
    addItem(NewSpring());
}

void WrappingRow::AddWidgetThatStepsDown(QWidget* widget)
{
    addWidget(widget);
    steppingDown_.insert(items_.last());
}

void WrappingRow::AddSpringOnTheLowerLine()
{
    addItem(NewSpring());
    steppingDown_.insert(items_.last());
    onlyOnTheLowerLine_.insert(items_.last());
}

void WrappingRow::AddWidgetThatHoldsTheUpperLine(QWidget* widget)
{
    addWidget(widget);
    holdingTheUpperLine_ = items_.last();
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
    if (index < 0 || index >= items_.size())
    {
        return nullptr;
    }

    QLayoutItem* taken = items_.takeAt(index);
    steppingDown_.remove(taken);
    onlyOnTheLowerLine_.remove(taken);
    if (holdingTheUpperLine_ == taken)
    {
        holdingTheUpperLine_ = nullptr;
    }

    return taken;
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

    for (QLayoutItem* item : items_)
    {
        if (onlyOnTheLowerLine_.contains(item))
        {
            item->setGeometry(QRect());
        }
    }

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

    return {WidthInOneLine(ItemsOnOneLine()) + around.left() + around.right(),
            TallestIn(ItemsPresent()) + around.top() + around.bottom()};
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

int WrappingRow::WidthInOneLine(const QList<QLayoutItem*>& items) const
{
    int width = Gap() * std::max(0, static_cast<int>(items.size()) - 1);
    for (const QLayoutItem* item : items)
    {
        width += item->sizeHint().width();
    }

    return width;
}

QList<QLayoutItem*> WrappingRow::ItemsPresent() const
{
    QList<QLayoutItem*> present;
    std::ranges::copy_if(items_, std::back_inserter(present),
                         [](const QLayoutItem* item)
                         {
                             return !ItIsHidden(item);
                         });

    return present;
}

QList<QLayoutItem*> WrappingRow::ItemsOnOneLine() const
{
    QList<QLayoutItem*> kept;
    std::ranges::copy_if(ItemsPresent(), std::back_inserter(kept),
                         [this](const QLayoutItem* item)
                         {
                             return !onlyOnTheLowerLine_.contains(item);
                         });

    return kept;
}

bool WrappingRow::TheUpperLineIsHeld() const
{
    return holdingTheUpperLine_ != nullptr && !ItIsHidden(holdingTheUpperLine_);
}

bool WrappingRow::TheRestMustStepDown(const QList<QLayoutItem*>& oneLine, const int width) const
{
    return !steppingDown_.isEmpty() && (TheUpperLineIsHeld() || WidthInOneLine(oneLine) > width);
}

QList<QList<QLayoutItem*>> WrappingRow::WrapInOrder(const QList<QLayoutItem*>& items, const int width) const
{
    QList<QList<QLayoutItem*>> lines;
    QList<QLayoutItem*> line;
    int taken = 0;

    for (QLayoutItem* item : items)
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

QList<QList<QLayoutItem*>> WrappingRow::LinesThatFit(const int width) const
{
    const QList<QLayoutItem*> oneLine = ItemsOnOneLine();
    if (!TheRestMustStepDown(oneLine, width))
    {
        return WrapInOrder(oneLine, width);
    }

    QList<QLayoutItem*> staying;
    QList<QLayoutItem*> down;
    for (QLayoutItem* item : ItemsPresent())
    {
        if (steppingDown_.contains(item))
        {
            down.append(item);
        }
        else
        {
            staying.append(item);
        }
    }

    return WrapInOrder(staying, width) + WrapInOrder(down, width);
}

void WrappingRow::PlaceTheLine(const QList<QLayoutItem*>& row, const QRect& where) const
{
    QList<QLayoutItem*> takers;
    std::ranges::copy_if(row, std::back_inserter(takers), ItTakesTheSlack);
    std::ranges::stable_sort(takers, std::less{}, HowMuchMoreItWillTake);

    QHash<const QLayoutItem*, int> extra;
    int stillToGive = std::max(0, where.width() - WidthInOneLine(row));
    for (qsizetype at = 0; at < takers.size(); ++at)
    {
        const int fairShare = stillToGive / static_cast<int>(takers.size() - at);
        const int share = std::min(fairShare, HowMuchMoreItWillTake(takers[at]));
        extra.insert(takers[at], share);
        stillToGive -= share;
    }

    int x = where.x();
    for (QLayoutItem* item : row)
    {
        const int width = item->sizeHint().width() + extra.value(item);
        item->setGeometry(QRect(x, where.y(), width, where.height()));
        x += width + Gap();
    }
}
