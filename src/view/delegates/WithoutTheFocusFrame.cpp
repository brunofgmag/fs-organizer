#include "view/delegates/WithoutTheFocusFrame.h"

namespace
{
    constexpr int kAirAroundALine = 4;
}

QSize WithoutTheFocusFrame::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& line) const
{
    return QStyledItemDelegate::sizeHint(option, line) + QSize(0, kAirAroundALine);
}

void WithoutTheFocusFrame::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& line) const
{
    QStyledItemDelegate::initStyleOption(option, line);

    option->state &= ~QStyle::State_HasFocus;
}
