#include "viewmodel/AddonTreeModel.h"

#include <algorithm>
#include <utility>

#include "domain/linking/EntryClassifier.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/EffectiveDestination.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    QString Show(const std::filesystem::path& path)
    {
        return QString::fromStdWString(path.wstring());
    }

    Qt::CheckState ToQt(const CheckState state)
    {
        switch (state)
        {
        case CheckState::Checked: return Qt::Checked;
        case CheckState::Partial: return Qt::PartiallyChecked;
        case CheckState::Unchecked: break;
        }

        return Qt::Unchecked;
    }
}

AddonTreeModel::AddonTreeModel(QObject* parent) : QAbstractItemModel(parent)
{
}

void AddonTreeModel::ShowSnapshot(ProfileSnapshot snapshot, SimulatorProfile profile)
{
    beginResetModel();

    snapshot_ = std::move(snapshot);
    profile_ = std::move(profile);
    Rebuild();

    endResetModel();
}

void AddonTreeModel::RefreshEnabled(std::vector<DestinationEntry> entries)
{
    snapshot_.entries = std::move(entries);
    snapshot_.enabled = EnabledAddons(EnabledAddonFolders(snapshot_.entries));

    AnnounceValues({});
}

void AddonTreeModel::ShowProfile(SimulatorProfile profile)
{
    profile_ = std::move(profile);

    AnnounceValues({});
}

const ProfileSnapshot& AddonTreeModel::Snapshot() const
{
    return snapshot_;
}

const TreeNode* AddonTreeModel::NodeAt(const QModelIndex& position)
{
    return position.isValid() ? static_cast<const Item*>(position.internalPointer())->node : nullptr;
}

void AddonTreeModel::Rebuild()
{
    items_.clear();
    roots_.clear();

    for (const TreeNode& library : snapshot_.libraries)
    {
        roots_.push_back(AddItem(library, nullptr));
    }
}

AddonTreeModel::Item* AddonTreeModel::AddItem(const TreeNode& node, Item* parent)
{
    const int row = parent == nullptr
                        ? static_cast<int>(roots_.size())
                        : static_cast<int>(parent->children.size());

    items_.push_back(std::make_unique<Item>(Item{&node, parent, row, {}}));
    Item* item = items_.back().get();

    for (const TreeNode& child : node.children)
    {
        item->children.push_back(AddItem(child, item));
    }

    return item;
}

void AddonTreeModel::AnnounceValues(const QModelIndex& parent)
{
    const std::vector<Item*>& children = ChildrenOf(parent);
    if (children.empty())
    {
        return;
    }

    emit dataChanged(index(0, 0, parent), index(static_cast<int>(children.size()) - 1, 0, parent),
                     {Qt::CheckStateRole, Qt::DisplayRole});

    for (int row = 0; row < static_cast<int>(children.size()); ++row)
    {
        AnnounceValues(index(row, 0, parent));
    }
}

QString AddonTreeModel::NameOf(const TreeNode& node) const
{
    if (node.kind == TreeNodeKind::Library)
    {
        const Library* library = LibraryContaining(profile_, node.path);
        if (library != nullptr && !library->label.empty())
        {
            return QString::fromStdString(library->label);
        }
    }

    return Show(node.path.filename());
}

const std::vector<AddonTreeModel::Item*>& AddonTreeModel::ChildrenOf(const QModelIndex& parent) const
{
    return parent.isValid() ? static_cast<const Item*>(parent.internalPointer())->children : roots_;
}

QModelIndex AddonTreeModel::index(const int row, const int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
    {
        return {};
    }

    return createIndex(row, column, ChildrenOf(parent)[static_cast<std::size_t>(row)]);
}

QModelIndex AddonTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
    {
        return {};
    }

    const Item* parent = static_cast<const Item*>(child.internalPointer())->parent;

    return parent == nullptr ? QModelIndex{} : createIndex(parent->row, 0, parent);
}

int AddonTreeModel::rowCount(const QModelIndex& parent) const
{
    return parent.column() > 0 ? 0 : static_cast<int>(ChildrenOf(parent).size());
}

int AddonTreeModel::columnCount(const QModelIndex&) const
{
    return 1;
}

QVariant AddonTreeModel::data(const QModelIndex& position, const int role) const
{
    const TreeNode* node = NodeAt(position);
    if (node == nullptr)
    {
        return {};
    }

    if (role == Qt::CheckStateRole)
    {
        return ToQt(DeriveCheckState(*node, snapshot_.enabled));
    }

    if (role != Qt::DisplayRole)
    {
        return {};
    }

    const std::filesystem::path destination = EffectiveDestination(profile_, node->path);
    if (ComparablePath(destination) == ComparablePath(profile_.defaultDestination))
    {
        return NameOf(*node);
    }

    return tr("%1  →  %2").arg(NameOf(*node), Show(destination.filename()));
}

bool AddonTreeModel::setData(const QModelIndex& position, [[maybe_unused]] const QVariant& value,
                             const int role)
{
    const TreeNode* node = NodeAt(position);
    if (role != Qt::CheckStateRole || node == nullptr)
    {
        return false;
    }

    emit ToggleRequested(node);

    return false;
}

Qt::ItemFlags AddonTreeModel::flags(const QModelIndex& position) const
{
    if (!position.isValid())
    {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}
