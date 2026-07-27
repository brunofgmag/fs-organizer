#include "viewmodel/AddonTreeModel.h"

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/EffectiveDestination.h"
#include "domain/tree/LibraryLookup.h"
#include "support/PathText.h"

namespace
{
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

void AddonTreeModel::Show(const ProfileSnapshot& snapshot, const SimulatorProfile& profile)
{
    beginResetModel();

    libraries_ = snapshot.libraries;
    enabled_ = snapshot.enabled;
    conflicts_ = snapshot.conflicts;
    profile_ = profile;
    Rebuild();

    endResetModel();
}

void AddonTreeModel::Refresh(const ProfileSnapshot& snapshot, const SimulatorProfile& profile)
{
    enabled_ = snapshot.enabled;
    conflicts_ = snapshot.conflicts;
    profile_ = profile;

    AnnounceValues({});
}

const TreeNode* AddonTreeModel::NodeAt(const QModelIndex& position)
{
    return position.isValid() ? static_cast<const Item*>(position.internalPointer())->node : nullptr;
}

void AddonTreeModel::Rebuild()
{
    items_.clear();
    roots_.clear();

    for (const TreeNode& library : libraries_)
    {
        roots_.push_back(AddItem(library, nullptr));
    }
}

AddonTreeModel::Item* AddonTreeModel::AddItem(const TreeNode& node, Item* parent)
{
    const int row = parent == nullptr ? static_cast<int>(roots_.size()) : static_cast<int>(parent->children.size());

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

    return AsText(node.path.filename());
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

    const CopyConflict* conflict = conflicts_.OverTheLibraryAddon(node->path);

    if (role == Qt::CheckStateRole)
    {
        return ToQt(DeriveCheckState(*node, enabled_));
    }

    if (role == ConflictRole)
    {
        return conflict != nullptr;
    }

    if (role == ConflictDetailsRole)
    {
        return conflict == nullptr ? QVariant() : QVariant::fromValue(*conflict);
    }

    if (role == EnabledRole)
    {
        return enabled_.Contains(node->path);
    }

    if (role == Qt::ToolTipRole)
    {
        return conflict == nullptr
            ? QVariant()
            : tr("Já existe uma pasta de verdade com esse nome no destino: %1").arg(AsText(conflict->destinationPath));
    }

    if (role != Qt::DisplayRole)
    {
        return {};
    }

    const QString name = conflict == nullptr ? NameOf(*node) : tr("%1 (em conflito)").arg(NameOf(*node));

    const std::filesystem::path destination = EffectiveDestination(profile_, node->path);
    if (ComparablePath(destination) == ComparablePath(profile_.defaultDestination))
    {
        return name;
    }

    return tr("%1  →  %2").arg(name, AsText(destination.filename()));
}

bool AddonTreeModel::setData(const QModelIndex& position, [[maybe_unused]] const QVariant& value, const int role)
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
