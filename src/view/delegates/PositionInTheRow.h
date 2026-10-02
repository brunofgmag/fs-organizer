#ifndef FS_ORGANIZER_VIEW_DELEGATES_POSITION_IN_THE_ROW_H
#define FS_ORGANIZER_VIEW_DELEGATES_POSITION_IN_THE_ROW_H

#include <QtCore/QModelIndex>
#include <QtWidgets/QStyleOption>

void TellWhereTheCellSitsInTheRow(QStyleOptionViewItem& item, const QModelIndex& index);

#endif // FS_ORGANIZER_VIEW_DELEGATES_POSITION_IN_THE_ROW_H
