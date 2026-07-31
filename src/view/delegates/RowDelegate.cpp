#include "view/delegates/RowDelegate.h"

#include <algorithm>

#include <QtGui/QFontMetrics>
#include <QtGui/QHelpEvent>
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

    [[nodiscard]] RoomForTheText RoomIn(const QStyleOptionViewItem& item, const QModelIndex& index)
    {
        const QWidget* widget = item.widget;
        const QStyle* style = widget != nullptr ? widget->style() : QApplication::style();

        const QRect written = style->subElementRect(QStyle::SE_ItemViewItemText, &item, widget);
        const QRect box = written.adjusted(kBreathingRoom, 0, -kBreathingRoom, 0);

        const QString suffix = index.data(QuietSuffixRole).toString();
        const QString tag = index.data(TagTextRole).toString();

        const int tagRoom = tag.isEmpty() ? 0 : TagSizeOf(tag, item.font).width() + kBeforeTheTag;
        const int suffixRoom =
            suffix.isEmpty() ? 0 : QFontMetrics(item.font).horizontalAdvance(suffix) + kBeforeTheSuffix;

        return {box, std::max(0, box.width() - tagRoom - suffixRoom)};
    }

    [[nodiscard]] QString TextThatIsDrawn(const QStyleOptionViewItem& item, const QModelIndex& index)
    {
        return index.data(TagTextRole).toString() == item.text ? QString() : item.text;
    }
}

RowDelegate::RowDelegate(QObject* parent) : QStyledItemDelegate(parent)
{
}

void RowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem item = option;
    initStyleOption(&item, index);
    item.state &= ~QStyle::State_HasFocus;

    if (index.data(EmphasisRole).toBool())
    {
        item.font.setWeight(QFont::DemiBold);
    }

    if (index.data(AlarmingRole).toBool() && (item.state & QStyle::State_Selected) == 0)
    {
        item.backgroundBrush = AlarmingRowGround();
    }

    const QWidget* widget = item.widget;
    QStyle* style = widget != nullptr ? widget->style() : QApplication::style();

    const QString suffix = index.data(QuietSuffixRole).toString();
    const QString tag = index.data(TagTextRole).toString();
    const QString text = TextThatIsDrawn(item, index);
    const RoomForTheText room = RoomIn(item, index);

    item.text.clear();
    style->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);

    const QRect box = room.box;
    if (box.width() <= 0 || (text.isEmpty() && suffix.isEmpty() && tag.isEmpty()))
    {
        return;
    }

    const QFontMetrics measured(item.font);

    painter->save();
    painter->setFont(item.font);

    int pen = box.left();

    if (!text.isEmpty())
    {
        const QString fitted = measured.elidedText(text, item.textElideMode, room.wide);

        painter->setPen(InkFor(item, index));
        painter->drawText(box, static_cast<int>(item.displayAlignment), fitted);
        pen += measured.horizontalAdvance(fitted) + kBeforeTheSuffix;
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

    const QString text = TextThatIsDrawn(item, index);

    if (text.isEmpty() || QFontMetrics(item.font).horizontalAdvance(text) <= RoomIn(item, index).wide)
    {
        QToolTip::hideText();
        return false;
    }

    QToolTip::showText(event->globalPos(), text, view);

    return true;
}

QSize RowDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize wanted = QStyledItemDelegate::sizeHint(option, index);
    wanted.setWidth(wanted.width() + 2 * kBreathingRoom);

    if (const QString suffix = index.data(QuietSuffixRole).toString(); !suffix.isEmpty())
    {
        wanted.setWidth(wanted.width() + QFontMetrics(option.font).horizontalAdvance(suffix) + kBeforeTheSuffix);
    }

    if (const QString tag = index.data(TagTextRole).toString(); !tag.isEmpty())
    {
        wanted.setWidth(wanted.width() + TagSizeOf(tag, option.font).width() + kBeforeTheTag);
    }

    wanted.setHeight(std::max(wanted.height(), kRowHeight));

    return wanted;
}
