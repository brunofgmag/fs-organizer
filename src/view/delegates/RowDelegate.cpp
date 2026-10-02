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

#include "view/delegates/PositionInTheRow.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/TagTone.h"

namespace
{
    constexpr int kBreathingRoom = 8;
    constexpr int kBeforeTheTag = 8;
    constexpr int kBeforeTheSuffix = 7;
    constexpr int kBetweenTheTwoLines = 4;
    constexpr int kRowHeight = 29;
    constexpr int kProbeWidth = 200;

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

    [[nodiscard]] QStyle* StyleOf(const QStyleOptionViewItem& item)
    {
        return item.widget != nullptr ? item.widget->style() : QApplication::style();
    }

    [[nodiscard]] RoomForTheText RoomIn(const QStyleOptionViewItem& item,
                                        const QString& suffix,
                                        const QString& tag,
                                        const FittedText& fitted,
                                        const int shift)
    {
        const QWidget* widget = item.widget;
        const QStyle* style = StyleOf(item);

        const QRect written = style->subElementRect(QStyle::SE_ItemViewItemText, &item, widget);
        const QRect box = written.adjusted(kBreathingRoom + shift, 0, -kBreathingRoom, 0);

        const int tagRoom = tag.isEmpty() ? 0 : TagSizeOf(tag, item.font).width() + kBeforeTheTag;
        const int suffixRoom = suffix.isEmpty() ? 0 : fitted.AdvanceOf(suffix, item.font) + kBeforeTheSuffix;

        return {.box = box, .wide = std::max(0, box.width() - tagRoom - suffixRoom)};
    }

    void MeasureInTheFontTheCanvasDraws(QStyleOptionViewItem& item)
    {
        const auto* view = qobject_cast<const QAbstractItemView*>(item.widget);
        const QWidget* canvas = view != nullptr ? view->viewport() : item.widget;

        if (canvas != nullptr)
        {
            item.font = item.font.resolve(canvas->font());
        }

        item.font.setResolveMask(QFont::AllPropertiesResolved);
        item.fontMetrics = QFontMetrics(item.font);
    }

    void DrawWithTheCheckMovedBy(const int shift, QStyleOptionViewItem item, QPainter& painter)
    {
        const QWidget* widget = item.widget;
        QStyle* style = StyleOf(item);

        QStyleOptionViewItem check = item;
        check.rect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &item, widget).translated(shift, 0);
        check.state &= ~QStyle::State_HasFocus;
        check.state &= ~(QStyle::State_On | QStyle::State_Off | QStyle::State_NoChange);

        switch (item.checkState)
        {
        case Qt::Checked: check.state |= QStyle::State_On; break;
        case Qt::PartiallyChecked: check.state |= QStyle::State_NoChange; break;
        case Qt::Unchecked: check.state |= QStyle::State_Off; break;
        }

        item.features &= ~QStyleOptionViewItem::HasCheckIndicator;
        style->drawControl(QStyle::CE_ItemViewItem, &item, &painter, widget);
        style->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &check, &painter, widget);
    }

    [[nodiscard]] QString TextThatIsDrawn(const QStyleOptionViewItem& item, const QString& tag)
    {
        return tag == item.text ? QString() : item.text;
    }

    struct TwoLines
    {
        QRect first{};
        QRect second{};
    };

    [[nodiscard]] TwoLines LinesIn(const QRect& box, const int line, const bool setAsABlock)
    {
        if (!setAsABlock)
        {
            QRect first = box;
            first.setBottom(box.center().y());

            return {.first = first, .second = QRect(box.left(), box.center().y(), box.width(), line)};
        }

        const int above = std::max(0, (box.height() - 2 * line - kBetweenTheTwoLines) / 2);
        const QRect first(box.left(), box.top() + above, box.width(), line);

        return {.first = first,
                .second = QRect(box.left(), first.bottom() + 1 + kBetweenTheTwoLines, box.width(), line)};
    }

    [[nodiscard]] QString BothLinesOf(const QString& text, const QString& second)
    {
        QStringList whole;

        for (const QString& line : {text, second})
        {
            if (!line.isEmpty())
            {
                whole.append(line);
            }
        }

        return whole.join(QLatin1Char('\n'));
    }

    [[nodiscard]] QRect WhereTheTagGoes(const QSize& wanted, const QRect& box, const int pen, const bool atTheEnd)
    {
        const int farthest = std::max(box.left(), box.right() - wanted.width() + 1);

        QRect where(0, 0, wanted.width(), wanted.height());
        where.moveLeft(atTheEnd ? farthest : std::clamp(pen, box.left(), farthest));
        where.moveTop(box.center().y() - wanted.height() / 2 + 1);

        return where;
    }

    void RepaintTheRowOf(const QAbstractItemView& view, const QModelIndex& index)
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

void RowDelegate::AlignTheCheckWithTheText()
{
    checkAlignedWithText_ = true;
}

void RowDelegate::LetTheFirstCellLeadTheRow()
{
    firstCellLeadsTheRow_ = true;
}

int RowDelegate::CheckShiftOf(const QStyleOptionViewItem& item) const
{
    if (!checkAlignedWithText_ || (item.features & QStyleOptionViewItem::HasCheckIndicator) == 0)
    {
        return 0;
    }

    const QWidget* widget = item.widget;
    const QStyle* style = StyleOf(item);

    QStyleOptionViewItem cell = item;
    cell.rect = QRect(0, 0, kProbeWidth, kRowHeight);

    const QRect check = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &cell, widget);

    cell.features &= ~QStyleOptionViewItem::HasCheckIndicator;
    const QRect text = style->subElementRect(QStyle::SE_ItemViewItemText, &cell, widget);

    return std::max(0, text.left() + kBreathingRoom - check.left());
}

bool RowDelegate::IsOnTheCheck(const QModelIndex& index, const QPoint& at) const
{
    const auto* view = qobject_cast<const QAbstractItemView*>(parent());

    if (view == nullptr || !index.isValid() || !index.data(Qt::CheckStateRole).isValid())
    {
        return false;
    }

    QStyleOptionViewItem item;
    item.initFrom(view->viewport());
    item.widget = view;
    item.rect = view->visualRect(index);
    initStyleOption(&item, index);
    item.rect.adjust(CheckShiftOf(item), 0, 0, 0);

    return view->style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &item, view).contains(at);
}

bool RowDelegate::DoubleClickedTheCheck() const
{
    return doubleClickedTheCheck_;
}

QStyleOptionViewItem RowDelegate::ItemAsDrawn(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem item = option;
    initStyleOption(&item, index);

    if (index.data(EmphasisRole).toBool())
    {
        item.font.setWeight(QFont::DemiBold);
    }

    MeasureInTheFontTheCanvasDraws(item);

    return item;
}

bool RowDelegate::editorEvent(QEvent* event,
                              QAbstractItemModel* model,
                              const QStyleOptionViewItem& option,
                              const QModelIndex& index)
{
    QStyleOptionViewItem item = option;
    initStyleOption(&item, index);

    QStyleOptionViewItem moved = option;
    moved.rect.adjust(CheckShiftOf(item), 0, 0, 0);

    return QStyledItemDelegate::editorEvent(event, model, moved, index);
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
        else if (event->type() == QEvent::MouseButtonDblClick)
        {
            if (const auto* mouse = dynamic_cast<QMouseEvent*>(event); mouse != nullptr)
            {
                const QPoint at = mouse->position().toPoint();

                doubleClickedTheCheck_ = IsOnTheCheck(view->indexAt(at), at);
            }
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
    QStyleOptionViewItem item = ItemAsDrawn(option, index);
    item.state &= ~QStyle::State_HasFocus;
    TellWhereTheCellSitsInTheRow(item, index);

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
    QStyle* style = StyleOf(item);

    const QString suffix = index.data(QuietSuffixRole).toString();
    const QString tag = index.data(TagTextRole).toString();
    const QString second = index.data(SecondLineRole).toString();
    const QString text = TextThatIsDrawn(item, tag);
    const int shift = CheckShiftOf(item);
    const QModelIndex leading = firstCellLeadsTheRow_ ? index.siblingAtColumn(0) : QModelIndex();
    const bool ledByTwoLinesOfText = leading.isValid() && !leading.data(SecondLineRole).toString().isEmpty();
    const RoomForTheText room = RoomIn(item, suffix, tag, fitted_, shift);

    item.text.clear();

    if (shift == 0)
    {
        style->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);
    }
    else
    {
        DrawWithTheCheckMovedBy(shift, item, *painter);
    }

    QRect box = room.box;
    if (box.width() <= 0 || (text.isEmpty() && suffix.isEmpty() && tag.isEmpty() && second.isEmpty()))
    {
        return;
    }

    const QFontMetrics measured(item.font);

    painter->save();
    painter->setFont(item.font);

    const TwoLines lines = LinesIn(box, measured.height(), ledByTwoLinesOfText);

    if (!second.isEmpty())
    {
        painter->setPen(QuietInk());
        painter->drawText(lines.second, Qt::AlignLeft | Qt::AlignVCenter,
                          fitted_.In(second, item.font, Qt::ElideMiddle, box.width()));
    }

    if (!second.isEmpty() || ledByTwoLinesOfText)
    {
        box = lines.first;
    }

    int pen = box.left();

    if (!text.isEmpty())
    {
        const QString fitted = fitted_.In(text, item.font, item.textElideMode, room.wide);

        painter->setPen(InkFor(item, index));
        painter->drawText(box, static_cast<int>(item.displayAlignment), fitted);

        if (!suffix.isEmpty() || !tag.isEmpty())
        {
            pen += fitted_.AdvanceOf(fitted, item.font) + kBeforeTheSuffix;
        }
    }

    if (!suffix.isEmpty() && pen + fitted_.AdvanceOf(suffix, item.font) + kBeforeTheSuffix <= box.right())
    {
        painter->setPen(QuietInk());
        painter->drawText(QRect(pen, box.top(), box.right() - pen + 1, box.height()),
                          static_cast<int>(item.displayAlignment), suffix);
        pen += fitted_.AdvanceOf(suffix, item.font) + kBeforeTheTag;
    }

    if (!tag.isEmpty())
    {
        const QRect where = WhereTheTagGoes(TagSizeOf(tag, item.font), box, pen, index.data(TagAtTheEndRole).toBool());

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

    const QStyleOptionViewItem item = ItemAsDrawn(option, index);

    const QString suffix = index.data(QuietSuffixRole).toString();
    const QString tag = index.data(TagTextRole).toString();
    const QString text = TextThatIsDrawn(item, tag);
    const QString second = index.data(SecondLineRole).toString();
    const RoomForTheText room = RoomIn(item, suffix, tag, fitted_, CheckShiftOf(item));
    const QFontMetrics measured(item.font);

    const bool cropped = !text.isEmpty() && measured.horizontalAdvance(text) > room.wide;
    const bool croppedUnderneath = !second.isEmpty() && measured.horizontalAdvance(second) > room.box.width();

    if (!cropped && !croppedUnderneath)
    {
        QToolTip::hideText();
        return false;
    }

    QToolTip::showText(event->globalPos(), BothLinesOf(text, second), view);

    return true;
}

int RowDelegate::TimesItAskedTheFont() const
{
    return fitted_.TimesItAskedTheFont();
}

QSize RowDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem drawnIn = option;
    MeasureInTheFontTheCanvasDraws(drawnIn);

    QStyleOptionViewItem item = drawnIn;
    initStyleOption(&item, index);

    QSize wanted = QStyledItemDelegate::sizeHint(drawnIn, index);
    wanted.setWidth(wanted.width() + 2 * kBreathingRoom + CheckShiftOf(item));

    if (const QString suffix = index.data(QuietSuffixRole).toString(); !suffix.isEmpty())
    {
        wanted.setWidth(wanted.width() + QFontMetrics(drawnIn.font).horizontalAdvance(suffix) + kBeforeTheSuffix);
    }

    if (const QString tag = index.data(TagTextRole).toString(); !tag.isEmpty())
    {
        const QFontMetrics measured(drawnIn.font);
        const int dropped =
            measured.horizontalAdvance(item.text) - measured.horizontalAdvance(TextThatIsDrawn(item, tag));

        wanted.setWidth(wanted.width() - dropped + TagSizeOf(tag, drawnIn.font).width() + kBeforeTheTag);
    }

    if (!index.data(SecondLineRole).toString().isEmpty())
    {
        wanted.setHeight(wanted.height() + QFontMetrics(drawnIn.font).height() + kBreathingRoom);
    }

    wanted.setHeight(std::max(wanted.height(), shortestRow_));

    return wanted;
}
