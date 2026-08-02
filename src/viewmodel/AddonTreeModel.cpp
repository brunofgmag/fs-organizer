#include "viewmodel/AddonTreeModel.h"

#include <algorithm>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/DestinationDivergence.h"
#include "domain/tree/EffectiveDestination.h"
#include "domain/tree/LibraryLookup.h"
#include "support/PathText.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/TagTone.h"

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
    entries_ = snapshot.entries;
    enabled_ = snapshot.enabled;
    conflicts_ = snapshot.conflicts;
    profile_ = profile;
    Rebuild();

    endResetModel();
}

void AddonTreeModel::Refresh(const ProfileSnapshot& snapshot, const SimulatorProfile& profile)
{
    entries_ = snapshot.entries;
    enabled_ = snapshot.enabled;
    conflicts_ = snapshot.conflicts;
    profile_ = profile;

    AnnounceValues({});
}

const TreeNode* AddonTreeModel::NodeAt(const QModelIndex& position)
{
    return position.isValid() ? static_cast<const Item*>(position.internalPointer())->node : nullptr;
}

std::size_t AddonTreeModel::AddonCount() const
{
    std::size_t count = 0;
    for (const std::unique_ptr<Item>& item : items_)
    {
        if (item->node->kind == TreeNodeKind::Addon)
        {
            ++count;
        }
    }

    return count;
}

std::size_t AddonTreeModel::EnabledCount() const
{
    std::size_t count = 0;
    for (const std::unique_ptr<Item>& item : items_)
    {
        if (item->node->kind == TreeNodeKind::Addon && enabled_.Contains(item->node->path))
        {
            ++count;
        }
    }

    return count;
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

    emit dataChanged(index(0, 0, parent), index(static_cast<int>(children.size()) - 1, Columns - 1, parent));

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

QString AddonTreeModel::CountedSuffixOf(const TreeNode& node)
{
    if (node.kind == TreeNodeKind::Addon)
    {
        return {};
    }

    const std::size_t addons = CountAddons(node);

    if (node.kind == TreeNodeKind::Category)
    {
        return QString::number(addons);
    }

    return tr("%1 · %2").arg(tr("%n category", nullptr, static_cast<int>(CountCategoriesInside(node))),
                             tr("%n addon", nullptr, static_cast<int>(addons)));
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
    return Columns;
}

QVariant AddonTreeModel::data(const QModelIndex& position, const int role) const
{
    const TreeNode* node = NodeAt(position);
    if (node == nullptr)
    {
        return {};
    }

    const CopyConflict* conflict = conflicts_.OverTheLibraryAddon(node->path);
    const bool broken = LinksNowhere(*node);

    if (role == Qt::CheckStateRole)
    {
        return position.column() == AddonColumn ? QVariant(ToQt(DeriveCheckState(*node, enabled_))) : QVariant();
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

    if (role == DivergentRole)
    {
        return !WhereItIsLinked(*node).empty();
    }

    if (role == BrokenRole)
    {
        return broken;
    }

    if (role == AlarmingRole)
    {
        return broken || conflict != nullptr;
    }

    if (role == TagTextRole)
    {
        if (position.column() != AddonColumn)
        {
            return {};
        }

        if (broken)
        {
            return tr("No target");
        }

        return conflict == nullptr ? QVariant() : QVariant(tr("In conflict"));
    }

    if (role == TagToneRole)
    {
        return static_cast<int>(broken ? TagTone::Filled : TagTone::Outlined);
    }

    if (role == EmphasisRole)
    {
        return position.column() == AddonColumn && node->kind != TreeNodeKind::Addon;
    }

    if (role == QuietSuffixRole)
    {
        return position.column() == AddonColumn ? QVariant(CountedSuffixOf(*node)) : QVariant();
    }

    if (role == QuietRole)
    {
        return position.column() == VersionColumn || position.column() == DestinationColumn;
    }

    if (role == AlertRole)
    {
        return position.column() == DestinationColumn && WandersFromTheDefault(*node);
    }

    if (role == Qt::ToolTipRole)
    {
        return ToolTipOf(*node, conflict);
    }

    if (role != Qt::DisplayRole)
    {
        return {};
    }

    return DisplayTextOf(*node, position.column());
}

QVariant AddonTreeModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return {};
    }

    switch (section)
    {
    case AddonColumn: return tr("Addon");
    case VersionColumn: return tr("Version");
    case DestinationColumn: return tr("Destination");
    default: return {};
    }
}

QString AddonTreeModel::DisplayTextOf(const TreeNode& node, const int column) const
{
    switch (column)
    {
    case AddonColumn: return NameOf(node);

    case VersionColumn:
        return node.addon.has_value() ? QString::fromStdString(node.addon->manifest.packageVersion) : QString();

    case DestinationColumn:
    {
        const std::filesystem::path strayed = WhereItIsLinked(node);
        if (!strayed.empty())
        {
            return AsText(strayed.filename());
        }

        const std::filesystem::path destination = EffectiveDestination(profile_, node.path);
        if (destination.empty())
        {
            return {};
        }

        return ComparablePath(destination) == ComparablePath(profile_.defaultDestination)
            ? AsText(destination.filename())
            : tr("%1 · pinned").arg(AsText(destination.filename()));
    }

    default: return {};
    }
}

QString AddonTreeModel::ToolTipOf(const TreeNode& node, const CopyConflict* conflict) const
{
    if (conflict != nullptr)
    {
        return tr("There is already a real folder with that name in the destination: %1")
            .arg(AsText(conflict->destinationPath));
    }

    const std::filesystem::path linked = WhereItIsLinked(node);
    if (linked.empty())
    {
        return {};
    }

    return tr("This addon is linked in %1, not in the destination the profile says to use, which is %2.")
        .arg(AsText(linked), AsText(EffectiveDestination(profile_, node.path)));
}

std::filesystem::path AddonTreeModel::WhereItIsLinked(const TreeNode& node) const
{
    return node.kind == TreeNodeKind::Addon ? DestinationItStrayedTo(profile_, entries_, node.path)
                                            : std::filesystem::path{};
}

bool AddonTreeModel::WandersFromTheDefault(const TreeNode& node) const
{
    if (!WhereItIsLinked(node).empty())
    {
        return true;
    }

    const std::filesystem::path destination = EffectiveDestination(profile_, node.path);

    return !destination.empty() && ComparablePath(destination) != ComparablePath(profile_.defaultDestination);
}

bool AddonTreeModel::LinksNowhere(const TreeNode& node) const
{
    if (node.kind != TreeNodeKind::Addon || !enabled_.Contains(node.path))
    {
        return false;
    }

    const std::string wanted = ComparablePath(EffectiveDestination(profile_, node.path) / node.path.filename());

    return std::ranges::any_of(entries_,
                               [&wanted](const DestinationEntry& entry)
                               {
                                   return entry.classification == EntryClassification::Broken
                                       && ComparablePath(entry.path) == wanted;
                               });
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

    constexpr Qt::ItemFlags common = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    return position.column() == AddonColumn ? common | Qt::ItemIsUserCheckable : common;
}
