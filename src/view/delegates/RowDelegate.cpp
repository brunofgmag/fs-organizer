#include "view/delegates/RowDelegate.h"

#include <algorithm>

#include <QtCore/QStringList>
#include <QtGui/QFontMetrics>
#include <QtGui/QHelpEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QApplication>
#include <QtWidgets/QToolTip>

#include "view/theme/ModernistPaint.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/TagTone.h"

namespace
{
    constexpr int kBreathingRoom = 8;
    constexpr int kBeforeTheTag = 8;
    constexpr int kBeforeTheSuffix = 7;
    constexpr int kRowHeight = 29;

    QPalette::ColorGroup GroupFor(const QStyle::State state)
    {
        if ((state & QStyle::State_Enabled) == 0)
        {
            return QPalette::Disabled;
        }

        return (state & QStyle::State_Active) != 0 ? QPalette::Normal : QPalette::Inactive;
    }

    QColor InkFor(const QStyleOptionViewItem& item, const QModelIndex& index)
    {
        if (index.data(AlertRole).toBool())
        {
            return AlertInk();
        }

        if (index.data(QuietRole).toBool())
        {
            return QuietInk();
        }

        const QPalette::ColorRole role =
            (item.state & QStyle::State_Selected) != 0 ? QPalette::HighlightedText : QPalette::Text;

        return item.palette.color(GroupFor(item.state), role);
    }

    struct RoomForTheText
    {
        QRect box;
        int wide = 0;
    };

    [[nodiscard]] RoomForTheText RoomIn(const QStyleOptionViewItem& item, const QString& suffix, const QString& tag)
    {
        const QWidget* widget = item.widget;
        const QStyle* style = widget != nullptr ? widget->style() : QApplication::style();

        const QRect written = style->subElementRect(QStyle::SE_ItemViewItemText, &item, widget);
        const QRect box = written.adjusted(kBreathingRoom, 0, -kBreathingRoom, 0);

        const int tagRoom = tag.isEmpty() ? 0 : TagSizeOf(tag, item.font).width() + kBeforeTheTag;
        const int suffixRoom =
            suffix.isEmpty() ? 0 : QFontMetrics(item.font).horizontalAdvance(suffix) + kBeforeTheSuffix;

        return {.box = box, .wide = std::max(0, box.width() - tagRoom - suffixRoom)};
    }

    [[nodiscard]] QString TextThatIsDrawn(const QStyleOptionViewItem& item, const QString& tag)
    {
        return tag == item.text ? QString() : item.text;
    }

    void RepaintTheRowOf(QAbstractItemView& view, const QModelIndex& index)
    {
        if (!index.isValid())
        {
            return;
        }

        QRect band = view.visualRect(index);

        if (band.isEmpty())
        {
            return;
        }

        band.setLeft(0);
        band.setRight(view.viewport()->width() - 1);

        view.viewport()->update(band);
    }

    [[nodiscard]] QStyleOptionViewItem::ViewItemPosition WhereInTheRow(const QModelIndex& index)
    {
        const int columns = index.model() == nullptr ? 1 : index.model()->columnCount(index.parent());

        if (columns <= 1)
        {
            return QStyleOptionViewItem::OnlyOne;
        }

        if (index.column() == 0)
        {
            return QStyleOptionViewItem::Beginning;
        }

        return index.column() == columns - 1 ? QStyleOptionViewItem::End : QStyleOptionViewItem::Middle;
    }
}

RowDelegate::RowDelegate(QObject* parent) : QStyledItemDelegate(parent), shortestRow_(kRowHeight)
{
    if (auto* view = qobject_cast<QAbstractItemView*>(parent); view != nullptr)
    {
        view->viewport()->setMouseTracking(true);
        view->viewport()->installEventFilter(this);
    }
}

void RowDelegate::KeepRowsAtLeast(const int tall)
{
    shortestRow_ = tall;
}

bool RowDelegate::eventFilter(QObject* watched, QEvent* event)
{
    const auto* view = qobject_cast<QAbstractItemView*>(parent());

    if (view != nullptr && watched == view->viewport())
    {
        if (event->type() == QEvent::MouseMove)
        {
            if (const auto* mouse = dynamic_cast<QMouseEvent*>(event); mouse != nullptr)
            {
                PointAt(view->indexAt(mouse->position().toPoint()));
            }
        }
        else if (event->type() == QEvent::Leave)
        {
            PointAt({});
        }
    }

    return QStyledItemDelegate::eventFilter(watched, event);
}

void RowDelegate::PointAt(const QModelIndex& index)
{
    if (IsPointedAt(index) && index.isValid() == pointedAt_.isValid())
    {
        return;
    }

    const QModelIndex left = pointedAt_;
    pointedAt_ = index;

    if (auto* view = qobject_cast<QAbstractItemView*>(parent()); view != nullptr)
    {
        RepaintTheRowOf(*view, left);
        RepaintTheRowOf(*view, index);
    }
}

bool RowDelegate::IsPointedAt(const QModelIndex& index) const
{
    return pointedAt_.isValid() && index.isValid() && pointedAt_.row() == index.row()
        && pointedAt_.parent() == index.parent();
}

void RowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem item = option;
    initStyleOption(&item, index);
    item.state &= ~QStyle::State_HasFocus;

    if (item.viewItemPosition == QStyleOptionViewItem::Invalid)
    {
        item.viewItemPosition = WhereInTheRow(index);
    }

    if (index.data(EmphasisRole).toBool())
    {
        item.font.setWeight(QFont::DemiBold);
    }

    if ((item.state & QStyle::State_Selected) == 0)
    {
        if (index.data(AlarmingRole).toBool())
        {
            item.backgroundBrush = AlarmingRowGround();
        }
        else if (IsPointedAt(index))
        {
            item.backgroundBrush = PointedAtRowGround();
        }
    }

    const QWidget* widget = item.widget;
    QStyle* style = widget != nullptr ? widget->style() : QApplication::style();

    const QString suffix = index.data(QuietSuffixRole).toString();
    const QString tag = index.data(TagTextRole).toString();
    const QString second = index.data(SecondLineRole).toString();
    const QString text = TextThatIsDrawn(item, tag);
    const RoomForTheText room = RoomIn(item, suffix, tag);

    item.text.clear();
    style->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);

    QRect box = room.box;
    if (box.width() <= 0 || (text.isEmpty() && suffix.isEmpty() && tag.isEmpty() && second.isEmpty()))
    {
        return;
    }

    const QFontMetrics measured(item.font);

    painter->save();
    painter->setFont(item.font);

    if (!second.isEmpty())
    {
        const QRect under(box.left(), box.center().y(), box.width(), measured.height());

        painter->setPen(QuietInk());
        painter->drawText(under, Qt::AlignLeft | Qt::AlignVCenter,
                          fitted_.In(second, item.font, Qt::ElideMiddle, box.width()));

        box.setBottom(box.center().y());
    }

    int pen = box.left();

    if (!text.isEmpty())
    {
        const QString fitted = fitted_.In(text, item.font, item.textElideMode, room.wide);

        painter->setPen(InkFor(item, index));
        painter->drawText(box, static_cast<int>(item.displayAlignment), fitted);

        if (!suffix.isEmpty() || !tag.isEmpty())
        {
            pen += measured.horizontalAdvance(fitted) + kBeforeTheSuffix;
        }
    }

    if (!suffix.isEmpty() && pen + measured.horizontalAdvance(suffix) + kBeforeTheSuffix <= box.right())
    {
        painter->setPen(QuietInk());
        painter->drawText(QRect(pen, box.top(), box.right() - pen + 1, box.height()),
                          static_cast<int>(item.displayAlignment), suffix);
        pen += measured.horizontalAdvance(suffix) + kBeforeTheTag;
    }

    if (!tag.isEmpty())
    {
        const QSize wanted = TagSizeOf(tag, item.font);

        QRect where(0, 0, wanted.width(), wanted.height());
        where.moveLeft(std::clamp(pen, box.left(), std::max(box.left(), box.right() - wanted.width() + 1)));
        where.moveTop(box.center().y() - wanted.height() / 2 + 1);

        PaintTag(*painter, where, tag, static_cast<TagTone>(index.data(TagToneRole).toInt()), item.font);
    }

    painter->restore();
}

bool RowDelegate::helpEvent(QHelpEvent* event,
                            QAbstractItemView* view,
                            const QStyleOptionViewItem& option,
                            const QModelIndex& index)
{
    if (event == nullptr || event->type() != QEvent::ToolTip || !index.data(Qt::ToolTipRole).toString().isEmpty())
    {
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }

    QStyleOptionViewItem item = option;
    initStyleOption(&item, index);

    if (index.data(EmphasisRole).toBool())
    {
        item.font.setWeight(QFont::DemiBold);
    }

    const QString suffix = index.data(QuietSuffixRole).toString();
    const QString tag = index.data(TagTextRole).toString();
    const QString text = TextThatIsDrawn(item, tag);
    const QString second = index.data(SecondLineRole).toString();
    const RoomForTheText room = RoomIn(item, suffix, tag);
    const QFontMetrics measured(item.font);

    const bool cropped = !text.isEmpty() && measured.horizontalAdvance(text) > room.wide;
    const bool croppedUnderneath = !second.isEmpty() && measured.horizontalAdvance(second) > room.box.width();

    if (!cropped && !croppedUnderneath)
    {
        QToolTip::hideText();
        return false;
    }

    QStringList whole;
    for (const QString& line : {text, second})
    {
        if (!line.isEmpty())
        {
            whole.append(line);
        }
    }

    QToolTip::showText(event->globalPos(), whole.join(QLatin1Char('\n')), view);

    return true;
}

QSize RowDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem item = option;
    initStyleOption(&item, index);

    QSize wanted = QStyledItemDelegate::sizeHint(option, index);
    wanted.setWidth(wanted.width() + 2 * kBreathingRoom);

    if (const QString suffix = index.data(QuietSuffixRole).toString(); !suffix.isEmpty())
    {
        wanted.setWidth(wanted.width() + QFontMetrics(option.font).horizontalAdvance(suffix) + kBeforeTheSuffix);
    }

    if (const QString tag = index.data(TagTextRole).toString(); !tag.isEmpty())
    {
        const QFontMetrics measured(option.font);
        const int dropped =
            measured.horizontalAdvance(item.text) - measured.horizontalAdvance(TextThatIsDrawn(item, tag));

        wanted.setWidth(wanted.width() - dropped + TagSizeOf(tag, option.font).width() + kBeforeTheTag);
    }

    if (!index.data(SecondLineRole).toString().isEmpty())
    {
        wanted.setHeight(wanted.height() + QFontMetrics(option.font).height() + kBreathingRoom);
    }

    wanted.setHeight(std::max(wanted.height(), shortestRow_));

    return wanted;
}
