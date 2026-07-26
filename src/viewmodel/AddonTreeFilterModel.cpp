#include "viewmodel/AddonTreeFilterModel.h"

#include "domain/tree/AddonTree.h"
#include "support/PathText.h"
#include "viewmodel/AddonTreeModel.h"

AddonTreeFilterModel::AddonTreeFilterModel(QObject* parent) : QSortFilterProxyModel(parent)
{
    setRecursiveFilteringEnabled(true);
}

void AddonTreeFilterModel::HideEmptyCategories(const bool hide)
{
    hideEmpty_ = hide;
    invalidateRowsFilter();
}

void AddonTreeFilterModel::Search(const QString& text)
{
    search_ = text.trimmed();
    invalidateRowsFilter();
}

bool AddonTreeFilterModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex position = sourceModel()->index(sourceRow, 0, sourceParent);
    const TreeNode* node = AddonTreeModel::NodeAt(position);

    if (node == nullptr)
    {
        return true;
    }

    if (node->kind == TreeNodeKind::Addon)
    {
        return search_.isEmpty() || AsText(node->path.filename()).contains(search_, Qt::CaseInsensitive);
    }

    if (hideEmpty_ && CountAddons(*node) == 0)
    {
        return false;
    }

    return search_.isEmpty();
}
