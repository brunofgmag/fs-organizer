#include "viewmodel/AddonTreeModel.h"

#include <algorithm>
#include <set>
#include <string>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
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
    destinations_.emplace(profile_, entries_);
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
    ReadEveryRow();

    endResetModel();
}

void AddonTreeModel::Retranslate()
{
    ReadEveryRow();

    emit layoutAboutToBeChanged();
    emit layoutChanged();
}

void AddonTreeModel::Refresh(const ProfileSnapshot& snapshot, const SimulatorProfile& profile)
{
    entries_ = snapshot.entries;
    enabled_ = snapshot.enabled;
    conflicts_ = snapshot.conflicts;
    profile_ = profile;
    ReadEveryRow();

    AnnounceValues({});
}

const TreeNode* AddonTreeModel::NodeAt(const QModelIndex& position)
{
    return position.isValid() ? static_cast<const Item*>(position.internalPointer())->node : nullptr;
}

const AddonTreeModel::Item* AddonTreeModel::ItemAt(const QModelIndex& position)
{
    return position.isValid() ? static_cast<const Item*>(position.internalPointer()) : nullptr;
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

SelectionTally AddonTreeModel::TallyOf(const std::vector<const TreeNode*>& nodes) const
{
    SelectionTally tally;
    std::set<std::string> reached;
    std::set<std::string> crossed;

    const auto reach = [this, &tally, &reached, &crossed](const TreeNode& addon)
    {
        if (!reached.insert(ComparablePath(addon.path)).second)
        {
            return;
        }

        const AddonDestination where = destinations_->Of(addon.path);
        const bool broken = enabled_.Contains(addon.path) && where.linksNowhere;

        tally.addons.push_back(addon.path);
        tally.enabled += enabled_.Contains(addon.path) ? 1 : 0;
        tally.broken += broken ? 1 : 0;
        tally.strayed += where.strayedTo.empty() ? 0 : 1;
        tally.alarming = tally.alarming || broken || conflicts_.OverTheLibraryAddon(addon.path) != nullptr;

        crossed.insert(ComparablePath(addon.path.parent_path()));
    };

    for (const TreeNode* node : nodes)
    {
        if (node == nullptr)
        {
            continue;
        }

        tally.categories += node->kind == TreeNodeKind::Addon ? 0 : 1;
        tally.alarming = tally.alarming || conflicts_.OverTheLibraryAddon(node->path) != nullptr;

        for (const TreeNode* addon : AddonsUnder(*node))
        {
            reach(*addon);
        }
    }

    tally.categoriesCrossed = crossed.size();

    return tally;
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

    items_.push_back(std::make_unique<Item>(Item{.node = &node, .parent = parent, .row = row, .children = {}}));
    Item* item = items_.back().get();

    for (const TreeNode& child : node.children)
    {
        item->children.push_back(AddItem(child, item));
    }

    return item;
}

void AddonTreeModel::ReadEveryRow()
{
    destinations_.emplace(profile_, entries_);

    for (const std::unique_ptr<Item>& item : items_)
    {
        item->reading = ReadingOf(*item->node);
    }
}

AddonTreeModel::Reading AddonTreeModel::ReadingOf(const TreeNode& node) const
{
    const AddonDestination where = destinations_->Of(node.path);
    const bool addon = node.kind == TreeNodeKind::Addon;
    const std::filesystem::path strayedTo = addon ? where.strayedTo : std::filesystem::path{};

    return {.name = NameOf(node),
            .conflict = conflicts_.OverTheLibraryAddon(node.path),
            .destination = where.destination,
            .strayedTo = strayedTo,
            .addons = addon ? 0 : CountAddons(node),
            .categories = node.kind == TreeNodeKind::Library ? CountCategoriesInside(node) : 0,
            .checked = ToQt(DeriveCheckState(node, enabled_)),
            .enabled = enabled_.Contains(node.path),
            .broken = addon && enabled_.Contains(node.path) && where.linksNowhere,
            .pinned = !where.destination.empty()
                && ComparablePath(where.destination) != ComparablePath(profile_.defaultDestination)};
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

QString AddonTreeModel::CountedSuffixOf(const TreeNode& node, const Reading& reading)
{
    if (node.kind == TreeNodeKind::Addon)
    {
        return {};
    }

    if (node.kind == TreeNodeKind::Category)
    {
        return QString::number(reading.addons);
    }

    return tr("%1 · %2").arg(tr("%n category", nullptr, static_cast<int>(reading.categories)),
                             tr("%n addon", nullptr, static_cast<int>(reading.addons)));
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
    const Item* item = ItemAt(position);
    if (item == nullptr)
    {
        return {};
    }

    const TreeNode& node = *item->node;
    const Reading& reading = item->reading;

    if (role == Qt::CheckStateRole)
    {
        return position.column() == AddonColumn ? QVariant(reading.checked) : QVariant();
    }

    if (role == ConflictRole)
    {
        return reading.conflict != nullptr;
    }

    if (role == ConflictDetailsRole)
    {
        return reading.conflict == nullptr ? QVariant() : QVariant::fromValue(*reading.conflict);
    }

    if (role == EnabledRole)
    {
        return reading.enabled;
    }

    if (role == DivergentRole)
    {
        return !reading.strayedTo.empty();
    }

    if (role == BrokenRole)
    {
        return reading.broken;
    }

    if (role == AlarmingRole)
    {
        return reading.broken || reading.conflict != nullptr;
    }

    if (role == TagTextRole)
    {
        if (position.column() != AddonColumn)
        {
            return {};
        }

        if (reading.broken)
        {
            return tr("No target");
        }

        if (reading.conflict == nullptr)
        {
            return {};
        }

        return QVariant(reading.conflict->theProvenanceIsAnotherProgram ? tr("Two copies") : tr("In conflict"));
    }

    if (role == TagToneRole)
    {
        return static_cast<int>(reading.broken ? TagTone::Filled : TagTone::Outlined);
    }

    if (role == EmphasisRole)
    {
        return position.column() == AddonColumn && node.kind != TreeNodeKind::Addon;
    }

    if (role == QuietSuffixRole)
    {
        return position.column() == AddonColumn ? QVariant(CountedSuffixOf(node, reading)) : QVariant();
    }

    if (role == QuietRole)
    {
        return position.column() == VersionColumn || position.column() == DestinationColumn;
    }

    if (role == AlertRole)
    {
        return position.column() == DestinationColumn && (!reading.strayedTo.empty() || reading.pinned);
    }

    if (role == Qt::ToolTipRole)
    {
        return ToolTipOf(reading);
    }

    if (role != Qt::DisplayRole)
    {
        return {};
    }

    return DisplayTextOf(node, reading, position.column());
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

QString AddonTreeModel::DisplayTextOf(const TreeNode& node, const Reading& reading, const int column) const
{
    switch (column)
    {
    case AddonColumn: return reading.name;

    case VersionColumn:
        return node.addon.has_value() ? QString::fromStdString(node.addon->manifest.packageVersion) : QString();

    case DestinationColumn:
    {
        if (!reading.strayedTo.empty())
        {
            return AsText(reading.strayedTo.filename());
        }

        if (reading.destination.empty())
        {
            return {};
        }

        return reading.pinned ? tr("%1 · pinned").arg(AsText(reading.destination.filename()))
                              : AsText(reading.destination.filename());
    }

    default: return {};
    }
}

QString AddonTreeModel::ToolTipOf(const Reading& reading) const
{
    if (reading.conflict != nullptr)
    {
        return reading.conflict->theProvenanceIsAnotherProgram
            ? tr("The other program took its folder back, so a second copy of this addon lives in: %1")
                  .arg(AsText(reading.conflict->provenancePath))
            : tr("There is already a real folder with that name in the destination: %1")
                  .arg(AsText(reading.conflict->provenancePath));
    }

    if (reading.strayedTo.empty())
    {
        return {};
    }

    return tr("This addon is linked in %1, not in the destination the profile says to use, which is %2.")
        .arg(AsText(reading.strayedTo), AsText(reading.destination));
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
