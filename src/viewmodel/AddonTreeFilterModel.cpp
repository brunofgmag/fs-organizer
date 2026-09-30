#include "viewmodel/AddonTreeFilterModel.h"

#include "domain/tree/AddonTree.h"
#include "support/PathText.h"
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/RowTagRoles.h"

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
    ForgetTheCounts();
    invalidateRowsFilter();
    AnnounceTheCounts({});
}

void AddonTreeFilterModel::ShowOnly(const AddonStateFilter state)
{
    state_ = state;
    ForgetTheCounts();
    invalidateRowsFilter();
    AnnounceTheCounts({});
}

AddonStateFilter AddonTreeFilterModel::ShowingOnly() const
{
    return state_;
}

bool AddonTreeFilterModel::HidesAddons() const
{
    return state_ != AddonStateFilter::All || !search_.isEmpty();
}

std::size_t AddonTreeFilterModel::TimesItWalkedTheTree() const
{
    return walks_;
}

void AddonTreeFilterModel::setSourceModel(QAbstractItemModel* model)
{
    for (const QMetaObject::Connection& connection : listening_)
    {
        disconnect(connection);
    }

    listening_.clear();
    ForgetTheCounts();

    if (model != nullptr)
    {
        listening_ = {connect(model, &QAbstractItemModel::dataChanged, this, &AddonTreeFilterModel::ForgetTheCounts),
                      connect(model, &QAbstractItemModel::modelReset, this, &AddonTreeFilterModel::ForgetTheCounts),
                      connect(model, &QAbstractItemModel::layoutChanged, this, &AddonTreeFilterModel::ForgetTheCounts),
                      connect(model, &QAbstractItemModel::rowsInserted, this, &AddonTreeFilterModel::ForgetTheCounts),
                      connect(model, &QAbstractItemModel::rowsRemoved, this, &AddonTreeFilterModel::ForgetTheCounts)};
    }

    QSortFilterProxyModel::setSourceModel(model);
}

QVariant AddonTreeFilterModel::data(const QModelIndex& position, const int role) const
{
    if (role != QuietSuffixRole || !HidesAddons() || position.column() != AddonTreeModel::AddonColumn)
    {
        return QSortFilterProxyModel::data(position, role);
    }

    const TreeNode* node = AddonTreeModel::NodeAt(mapToSource(position));
    const Reach* reach = node == nullptr || node->kind == TreeNodeKind::Addon ? nullptr : ReachOf(*node);

    if (reach == nullptr)
    {
        return QSortFilterProxyModel::data(position, role);
    }

    return CountTextOf(*node, *reach);
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
        return AnAddonShows(position, *node);
    }

    if (hideEmpty_ && CountAddons(*node) == 0)
    {
        return false;
    }

    return !HidesAddons();
}

bool AddonTreeFilterModel::TheStateAllows(const QModelIndex& position) const
{
    if (state_ == AddonStateFilter::All)
    {
        return true;
    }

    const bool enabled = position.data(Qt::CheckStateRole).toInt() == Qt::Checked;

    return enabled == (state_ == AddonStateFilter::Enabled);
}

bool AddonTreeFilterModel::AnAddonShows(const QModelIndex& position, const TreeNode& addon) const
{
    return (search_.isEmpty() || AsText(addon.path.filename()).contains(search_, Qt::CaseInsensitive))
        && TheStateAllows(position);
}

AddonTreeFilterModel::Reach AddonTreeFilterModel::WalkBelow(const QModelIndex& position) const
{
    const TreeNode* node = AddonTreeModel::NodeAt(position);
    Reach reach;

    if (node == nullptr)
    {
        return reach;
    }

    if (node->kind == TreeNodeKind::Addon)
    {
        reach.addons = 1;
        reach.addonsShown = AnAddonShows(position, *node) ? 1 : 0;

        return reach;
    }

    for (int row = 0; row < sourceModel()->rowCount(position); ++row)
    {
        const QModelIndex child = sourceModel()->index(row, 0, position);
        const Reach below = WalkBelow(child);
        const TreeNode* childNode = AddonTreeModel::NodeAt(child);
        const bool childIsACategory = childNode != nullptr && childNode->kind != TreeNodeKind::Addon;

        reach.addons += below.addons;
        reach.addonsShown += below.addonsShown;
        reach.categories += below.categories + (childIsACategory ? 1 : 0);
        reach.categoriesShown += below.categoriesShown + (childIsACategory && below.addonsShown > 0 ? 1 : 0);
    }

    reaches_[node] = reach;

    return reach;
}

const AddonTreeFilterModel::Reach* AddonTreeFilterModel::ReachOf(const TreeNode& node) const
{
    if (!counted_)
    {
        ++walks_;
        counted_ = true;

        for (int row = 0; row < sourceModel()->rowCount(); ++row)
        {
            static_cast<void>(WalkBelow(sourceModel()->index(row, 0, {})));
        }
    }

    const auto found = reaches_.find(&node);

    return found == reaches_.end() ? nullptr : &found->second;
}

QString AddonTreeFilterModel::CountTextOf(const TreeNode& node, const Reach& reach) const
{
    if (node.kind == TreeNodeKind::Category)
    {
        return tr("%1 of %2",
                  "addons shown out of all the addons under a category while a filter or a search is active")
            .arg(reach.addonsShown)
            .arg(reach.addons);
    }

    const QString addons =
        tr("%1 of %n addon", "addons shown out of all the addons of a library while a filter or a search is active",
           static_cast<int>(reach.addons))
            .arg(reach.addonsShown);

    const QString categories = tr("%1 of %n category",
                                  "categories shown out of all the categories of a library while a filter or a "
                                  "search is active",
                                  static_cast<int>(reach.categories))
                                   .arg(reach.categoriesShown);

    return tr("%1 · %2").arg(categories, addons);
}

void AddonTreeFilterModel::ForgetTheCounts()
{
    reaches_.clear();
    counted_ = false;
}

void AddonTreeFilterModel::AnnounceTheCounts(const QModelIndex& parent)
{
    const int rows = rowCount(parent);

    if (rows == 0)
    {
        return;
    }

    emit dataChanged(index(0, 0, parent), index(rows - 1, 0, parent), {QuietSuffixRole});

    for (int row = 0; row < rows; ++row)
    {
        const QModelIndex position = index(row, 0, parent);

        if (hasChildren(position))
        {
            AnnounceTheCounts(position);
        }
    }
}
