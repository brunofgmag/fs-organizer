#include "view/delegates/PositionInTheRow.h"

namespace
{
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

void TellWhereTheCellSitsInTheRow(QStyleOptionViewItem& item, const QModelIndex& index)
{
    if (item.viewItemPosition == QStyleOptionViewItem::Invalid)
    {
        item.viewItemPosition = WhereInTheRow(index);
    }
}
