#include "view/PlainTextDelegate.h"

#include <utility>

#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

namespace
{
    constexpr int kMostTextsWorthRemembering = 4096;

    QPalette::ColorGroup GroupFor(const QStyle::State state)
    {
        if ((state & QStyle::State_Enabled) == 0)
        {
            return QPalette::Disabled;
        }

        return (state & QStyle::State_Active) != 0 ? QPalette::Normal : QPalette::Inactive;
    }

    bool NeedsTheRichDelegate(const QStyleOptionViewItem& item)
    {
        return !item.icon.isNull() || (item.features & QStyleOptionViewItem::HasCheckIndicator) != 0
            || item.text.contains(QLatin1Char('\n'));
    }
}

PlainTextDelegate::PlainTextDelegate(QObject* parent) : QStyledItemDelegate(parent)
{
}

void PlainTextDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem item = option;
    initStyleOption(&item, index);

    if (NeedsTheRichDelegate(item))
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    const QString text = std::exchange(item.text, QString());
    const QWidget* widget = item.widget;
    QStyle* style = widget != nullptr ? widget->style() : QApplication::style();

    style->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);

    if (text.isEmpty())
    {
        return;
    }

    const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &item, widget) + 1;
    const QRect box = style->subElementRect(QStyle::SE_ItemViewItemText, &item, widget).adjusted(margin, 0, -margin, 0);
    const QPalette::ColorGroup group = GroupFor(item.state);
    const QPalette::ColorRole role =
        (item.state & QStyle::State_Selected) != 0 ? QPalette::HighlightedText : QPalette::Text;

    painter->save();
    painter->setFont(item.font);
    painter->setPen(item.palette.color(group, role));
    painter->drawText(box, static_cast<int>(item.displayAlignment), Fitted(text, item, box.width()));
    painter->restore();
}

QString PlainTextDelegate::Fitted(const QString& text, const QStyleOptionViewItem& item, const int width) const
{
    if (item.textElideMode == Qt::ElideNone || width <= 0)
    {
        return text;
    }

    if (measuredWith_ != item.font)
    {
        fitted_.clear();
        measuredWith_ = item.font;
    }

    QHash<QString, QString>& atThisWidth = fitted_[width];

    if (const auto remembered = atThisWidth.constFind(text); remembered != atThisWidth.constEnd())
    {
        return *remembered;
    }

    if (atThisWidth.size() >= kMostTextsWorthRemembering)
    {
        atThisWidth.clear();
    }

    return *atThisWidth.insert(text, QFontMetrics(item.font).elidedText(text, item.textElideMode, width));
}
