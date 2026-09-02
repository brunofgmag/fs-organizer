#include "view/TreeColumns.h"

#include <vector>

#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTreeWidget>

namespace
{
    void Widen(QTreeWidget* tree, const std::vector<int>& columns)
    {
        for (const int column : columns)
        {
            tree->resizeColumnToContents(column);
        }
    }
}

void LetTheseColumnsBeDragged(QTreeWidget* tree, const std::initializer_list<int> columns)
{
    const std::vector<int> theirs(columns);

    for (const int column : theirs)
    {
        tree->header()->setSectionResizeMode(column, QHeaderView::Interactive);
    }

    QObject::connect(tree, &QTreeWidget::itemExpanded, tree,
                     [tree, theirs]
                     {
                         Widen(tree, theirs);
                     });
}

void WidenTheseColumnsToTheirRows(QTreeWidget* tree, const std::initializer_list<int> columns)
{
    Widen(tree, std::vector<int>(columns));
}
