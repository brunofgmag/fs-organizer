#include "viewmodel/CommunityModel.h"

#include <utility>

#include "support/PathText.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/TagTone.h"

namespace
{
    TagTone ToneOf(const EntryClassification classification, const bool conflicted)
    {
        if (conflicted || classification == EntryClassification::Broken)
        {
            return classification == EntryClassification::Broken ? TagTone::Filled : TagTone::Outlined;
        }

        switch (classification)
        {
        case EntryClassification::Duplicated:
        case EntryClassification::Unmanaged: return TagTone::Outlined;
        case EntryClassification::External:
        case EntryClassification::Unavailable: return TagTone::Muted;
        case EntryClassification::Managed:
        case EntryClassification::Broken: break;
        }

        return TagTone::Line;
    }
}

CommunityModel::CommunityModel(QObject* parent) : QAbstractTableModel(parent)
{
}

QString CommunityModel::ClassificationName(const EntryClassification classification)
{
    switch (classification)
    {
    case EntryClassification::Managed: return tr("Managed");
    case EntryClassification::External: return tr("External");
    case EntryClassification::Broken: return tr("Broken");
    case EntryClassification::Unavailable: return tr("Unavailable");
    case EntryClassification::Unmanaged: return tr("Unmanaged");
    case EntryClassification::Duplicated: return tr("Duplicated");
    }

    return {};
}

void CommunityModel::ShowEntries(std::vector<DestinationEntry> entries,
                                 SimulatorProfile profile,
                                 CopyConflicts conflicts)
{
    beginResetModel();
    entries_ = std::move(entries);
    profile_ = std::move(profile);
    conflicts_ = std::move(conflicts);
    endResetModel();
}

const CopyConflict* CommunityModel::ConflictAt(const QModelIndex& position) const
{
    const DestinationEntry* entry = EntryAt(position);

    return entry == nullptr ? nullptr : conflicts_.OverTheDestinationEntry(entry->path);
}

const DestinationEntry* CommunityModel::EntryAt(const QModelIndex& position) const
{
    if (!position.isValid() || position.row() < 0 || static_cast<std::size_t>(position.row()) >= entries_.size())
    {
        return nullptr;
    }

    return &entries_[position.row()];
}

int CommunityModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(entries_.size());
}

int CommunityModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 4;
}

QVariant CommunityModel::data(const QModelIndex& position, const int role) const
{
    const DestinationEntry* entry = EntryAt(position);
    if (entry == nullptr)
    {
        return {};
    }

    const CopyConflict* conflict = conflicts_.OverTheDestinationEntry(entry->path);

    if (role == ClassificationRole)
    {
        return static_cast<int>(entry->classification);
    }

    if (role == ConflictRole)
    {
        return conflict != nullptr;
    }

    if (role == AlarmingRole)
    {
        return conflict != nullptr || entry->classification == EntryClassification::Broken
            || entry->classification == EntryClassification::Duplicated;
    }

    if (role == TagTextRole)
    {
        return position.column() == ClassificationColumn ? data(position, Qt::DisplayRole) : QVariant();
    }

    if (role == TagToneRole)
    {
        return static_cast<int>(ToneOf(entry->classification, conflict != nullptr));
    }

    if (role == QuietRole)
    {
        return position.column() == DestinationColumn || position.column() == TargetColumn;
    }

    if (role == Qt::ToolTipRole)
    {
        if (conflict == nullptr)
        {
            return {};
        }

        return tr("%1\nIt also exists in the library: %2")
            .arg(data(position, Qt::DisplayRole).toString(), AsText(conflict->libraryPath));
    }

    if (role != Qt::DisplayRole)
    {
        return {};
    }

    switch (position.column())
    {
    case NameColumn: return AsText(entry->path.filename());
    case DestinationColumn: return AsText(entry->path.parent_path().filename());
    case ClassificationColumn:
        return conflict == nullptr ? ClassificationName(entry->classification)
                                   : tr("%1 · in conflict").arg(ClassificationName(entry->classification));
    case TargetColumn: return entry->target.empty() ? QString() : AsText(entry->target.generic_wstring());
    default: return {};
    }
}

QVariant CommunityModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return {};
    }

    switch (section)
    {
    case NameColumn: return tr("Name");
    case DestinationColumn: return tr("Destination");
    case ClassificationColumn: return tr("Classification");
    case TargetColumn: return tr("Target");
    default: return {};
    }
}

CommunityFilterModel::CommunityFilterModel(QObject* parent) : QSortFilterProxyModel(parent)
{
}

void CommunityFilterModel::ShowOnly(const std::optional<EntryClassification> classification)
{
    classification_ = classification;
    conflictedOnly_ = false;
    invalidateRowsFilter();
}

void CommunityFilterModel::ShowOnlyTheConflicted(const bool only)
{
    conflictedOnly_ = only;
    classification_.reset();
    invalidateRowsFilter();
}

bool CommunityFilterModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex position = sourceModel()->index(sourceRow, 0, sourceParent);

    if (conflictedOnly_)
    {
        return sourceModel()->data(position, CommunityModel::ConflictRole).toBool();
    }

    if (!classification_.has_value())
    {
        return true;
    }

    return sourceModel()->data(position, CommunityModel::ClassificationRole).toInt()
        == static_cast<int>(*classification_);
}
