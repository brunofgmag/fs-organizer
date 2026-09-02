#ifndef FS_ORGANIZER_VIEW_TREE_COLUMNS_H
#define FS_ORGANIZER_VIEW_TREE_COLUMNS_H

#include <initializer_list>

class QTreeWidget;

void LetTheseColumnsBeDragged(QTreeWidget* tree, std::initializer_list<int> columns);

void WidenTheseColumnsToTheirRows(QTreeWidget* tree, std::initializer_list<int> columns);

#endif // FS_ORGANIZER_VIEW_TREE_COLUMNS_H
