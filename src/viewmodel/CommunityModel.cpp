#include "viewmodel/CommunityModel.h"

#include <utility>

#include <QtCore/QCoreApplication>

#include "support/PathText.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/TagTone.h"

namespace
{
    TagTone ToneOf(const EntryClassification classification, const bool conflicted)
    {
        if (classification == EntryClassification::Broken || classification == EntryClassification::Vanished
            || classification == EntryClassification::Substituted)
        {
            return TagTone::Filled;
        }

        if (conflicted)
        {
            return TagTone::Outlined;
        }

        switch (classification)
        {
        case EntryClassification::Substituted:
        case EntryClassification::Divergent:
        case EntryClassification::Duplicated:
        case EntryClassification::Unmanaged: return TagTone::Outlined;
        case EntryClassification::External:
        case EntryClassification::Unavailable: return TagTone::Muted;
        case EntryClassification::Managed:
        case EntryClassification::Vanished:
        case EntryClassification::Broken: break;
        }

        return TagTone::Line;
    }

    QString WhatTheStateMeans(const EntryClassification classification)
    {
        switch (classification)
        {
        case EntryClassification::Managed:
            return QCoreApplication::translate("CommunityModel", "the simulator loads it from your library");
        case EntryClassification::External:
            return QCoreApplication::translate("CommunityModel", "installed by another program");
        case EntryClassification::Divergent: return QCoreApplication::translate("CommunityModel", "two copies exist");
        case EntryClassification::Vanished:
            return QCoreApplication::translate("CommunityModel", "the library copy is gone");
        case EntryClassification::Broken:
            return QCoreApplication::translate("CommunityModel", "the link points to a folder that does not exist");
        case EntryClassification::Unavailable:
            return QCoreApplication::translate("CommunityModel", "the drive is not connected right now");
        case EntryClassification::Unmanaged:
            return QCoreApplication::translate("CommunityModel", "a regular folder, not in a library yet");
        case EntryClassification::Duplicated:
            return QCoreApplication::translate("CommunityModel", "linked in more than one destination");
        case EntryClassification::Substituted:
            return QCoreApplication::translate("CommunityModel", "the link was replaced by a regular folder");
        }

        return {};
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
    case EntryClassification::Divergent: return tr("Divergent");
    case EntryClassification::Vanished: return tr("Vanished");
    case EntryClassification::Broken: return tr("Broken");
    case EntryClassification::Unavailable: return tr("Unavailable");
    case EntryClassification::Unmanaged: return tr("Unmanaged");
    case EntryClassification::Duplicated: return tr("Duplicated");
    case EntryClassification::Substituted: return tr("Substituted");
    }

    return {};
}

void CommunityModel::ShowEntries(const std::vector<DestinationEntry>& entries, const CopyConflicts& conflicts)
{
    if (entries == entries_ && conflicts.All() == conflicts_.All())
    {
        return;
    }

    beginResetModel();
    entries_ = entries;
    conflicts_ = conflicts;
    ResolveTheConflictOfEachRow();
    endResetModel();
}

void CommunityModel::ResolveTheConflictOfEachRow()
{
    if (conflicts_.Count() == 0)
    {
        conflictOfRow_.assign(entries_.size(), nullptr);
        return;
    }

    conflictOfRow_.clear();
    conflictOfRow_.reserve(entries_.size());

    for (const DestinationEntry& entry : entries_)
    {
        conflictOfRow_.push_back(entry.theOtherProgramTookItsFolderBack
                                     ? conflicts_.OverTheProvenance(entry.externalOrigin)
                                     : conflicts_.OverTheProvenance(entry.path));
    }
}

const CopyConflict* CommunityModel::ConflictOnRow(const int row) const
{
    return conflictOfRow_[static_cast<std::size_t>(row)];
}

const CopyConflict* CommunityModel::ConflictAt(const QModelIndex& position) const
{
    return EntryAt(position) == nullptr ? nullptr : ConflictOnRow(position.row());
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

    if (role == ClassificationRole)
    {
        return static_cast<int>(entry->classification);
    }

    if (role == ConflictRole)
    {
        return ConflictOnRow(position.row()) != nullptr;
    }

    if (role == AlarmingRole)
    {
        return ConflictOnRow(position.row()) != nullptr || entry->classification == EntryClassification::Broken
            || entry->classification == EntryClassification::Duplicated
            || entry->classification == EntryClassification::Substituted
            || entry->classification == EntryClassification::Vanished;
    }

    if (role == SecondLineRole)
    {
        if (position.column() != NameColumn)
        {
            return {};
        }

        const std::filesystem::path& pointsAt = entry->externalOrigin.empty() ? entry->target : entry->externalOrigin;

        return pointsAt.empty() ? QVariant(AsText(entry->path)) : QVariant(AsText(pointsAt));
    }

    if (role == TagTextRole)
    {
        return position.column() == ClassificationColumn ? data(position, Qt::DisplayRole) : QVariant();
    }

    if (role == TagToneRole)
    {
        return static_cast<int>(ToneOf(entry->classification, ConflictOnRow(position.row()) != nullptr));
    }

    if (role == QuietRole)
    {
        return position.column() == DestinationColumn || position.column() == TargetColumn;
    }

    if (role == Qt::ToolTipRole)
    {
        const CopyConflict* conflict = ConflictOnRow(position.row());
        if (conflict == nullptr)
        {
            return {};
        }

        return tr("%1\nAlso in the library: %2")
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
    {
        const CopyConflict* conflict = ConflictOnRow(position.row());

        return conflict == nullptr || conflict->theProvenanceIsAnotherProgram || conflict->ourLinkWasReplaced
            ? ClassificationName(entry->classification)
            : tr("%1 · in conflict").arg(ClassificationName(entry->classification));
    }
    case TargetColumn: return WhatTheStateMeans(entry->classification);
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
    case TargetColumn: return tr("What this means");
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

void CommunityFilterModel::ShowOnlyWhatHolds(const QString& text)
{
    text_ = text;
    invalidateRowsFilter();
}

bool CommunityFilterModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    return TheKindIsWanted(sourceModel()->index(sourceRow, 0, sourceParent))
        && TheTextIsWanted(sourceRow, sourceParent);
}

bool CommunityFilterModel::TheTextIsWanted(const int sourceRow, const QModelIndex& sourceParent) const
{
    if (text_.isEmpty())
    {
        return true;
    }

    for (int column = 0; column < sourceModel()->columnCount(sourceParent); ++column)
    {
        const QModelIndex cell = sourceModel()->index(sourceRow, column, sourceParent);

        if (sourceModel()->data(cell, Qt::DisplayRole).toString().contains(text_, Qt::CaseInsensitive))
        {
            return true;
        }
    }

    return false;
}

bool CommunityFilterModel::TheKindIsWanted(const QModelIndex& position) const
{
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
