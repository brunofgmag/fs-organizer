#include "viewmodel/CommunityModel.h"

#include <utility>

namespace
{
    QString Show(const std::filesystem::path& path)
    {
        return QString::fromStdWString(path.wstring());
    }
}

CommunityModel::CommunityModel(QObject* parent) : QAbstractTableModel(parent)
{
}

void CommunityModel::ShowEntries(std::vector<DestinationEntry> entries, SimulatorProfile profile)
{
    beginResetModel();
    entries_ = std::move(entries);
    profile_ = std::move(profile);
    endResetModel();
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

    if (role != Qt::DisplayRole)
    {
        return {};
    }

    switch (position.column())
    {
    case NameColumn:
        return Show(entry->path.filename());
    case DestinationColumn:
        return Show(entry->path.parent_path().filename());
    case ClassificationColumn:
        switch (entry->classification)
        {
        case EntryClassification::Managed: return tr("Gerenciada");
        case EntryClassification::External: return tr("Externa");
        case EntryClassification::Broken: return tr("Quebrada");
        case EntryClassification::Unavailable: return tr("Indisponível");
        case EntryClassification::Unmanaged: return tr("Não gerenciada");
        case EntryClassification::Duplicated: return tr("Duplicada");
        }
        return {};
    case TargetColumn:
        return entry->target.empty() ? QString() : Show(entry->target.generic_wstring());
    default:
        return {};
    }
}

QVariant CommunityModel::headerData(const int section, const Qt::Orientation orientation,
                                    const int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return {};
    }

    switch (section)
    {
    case NameColumn: return tr("Nome");
    case DestinationColumn: return tr("Destino");
    case ClassificationColumn: return tr("Classificação");
    case TargetColumn: return tr("Alvo");
    default: return {};
    }
}

CommunityFilterModel::CommunityFilterModel(QObject* parent) : QSortFilterProxyModel(parent)
{
}

void CommunityFilterModel::ShowOnly(const std::optional<EntryClassification> classification)
{
    classification_ = classification;
    invalidateRowsFilter();
}

bool CommunityFilterModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    if (!classification_.has_value())
    {
        return true;
    }

    const QModelIndex position = sourceModel()->index(sourceRow, 0, sourceParent);

    return sourceModel()->data(position, CommunityModel::ClassificationRole).toInt()
        == static_cast<int>(*classification_);
}
