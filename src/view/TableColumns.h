#ifndef FS_ORGANIZER_VIEW_TABLE_COLUMNS_H
#define FS_ORGANIZER_VIEW_TABLE_COLUMNS_H

class QTableView;

void LetTheColumnsBeDraggedAndStillFillTheTable(QTableView* table, int columnThatTakesTheSlack = -1);

void LetTheColumnsFollowThoseOf(QTableView* follower, QTableView* followed);

#endif // FS_ORGANIZER_VIEW_TABLE_COLUMNS_H
