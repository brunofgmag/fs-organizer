#include "viewmodel/QuarantineModel.h"

#include <utility>

#include <QtCore/QDateTime>
#include <QtCore/QTimeZone>

#include "domain/support/PathUtils.h"
#include "support/PathText.h"
#include "support/SizeText.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/TagTone.h"

namespace
{
    QString Moment(const std::chrono::system_clock::time_point& timestamp)
    {
        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();

        return QDateTime::fromMSecsSinceEpoch(milliseconds, QTimeZone::UTC)
            .toLocalTime()
            .toString(QStringLiteral("dd/MM/yyyy HH:mm"));
    }
}

QuarantineModel::QuarantineModel(QObject* parent) : QAbstractTableModel(parent)
{
}

void QuarantineModel::ShowItems(std::vector<QuarantinedItem> items)
{
    beginResetModel();
    items_ = std::move(items);
    details_.clear();
    bytes_.clear();
    endResetModel();
}

void QuarantineModel::ShowDetails(const std::vector<QuarantineDetail>& details)
{
    for (const QuarantineDetail& detail : details)
    {
        details_[ComparablePath(detail.path)] = detail;
    }

    RepaintTheRows();
}

void QuarantineModel::ShowSizes(const std::vector<MeasuredFolder>& sizes)
{
    for (const MeasuredFolder& size : sizes)
    {
        if (size.measured)
        {
            bytes_[ComparablePath(size.folder)] = size.bytes;
        }
    }

    RepaintTheRows();
}

void QuarantineModel::RepaintTheRows()
{
    if (items_.empty())
    {
        return;
    }

    const int last = static_cast<int>(items_.size()) - 1;

    emit dataChanged(index(0, NameColumn, {}), index(last, WhereColumn, {}));
}

const QuarantinedItem* QuarantineModel::ItemAt(const QModelIndex& position) const
{
    if (!position.isValid() || position.row() < 0 || static_cast<std::size_t>(position.row()) >= items_.size())
    {
        return nullptr;
    }

    return &items_[static_cast<std::size_t>(position.row())];
}

const QuarantineDetail* QuarantineModel::DetailAt(const QModelIndex& position) const
{
    const QuarantinedItem* item = ItemAt(position);
    if (item == nullptr)
    {
        return nullptr;
    }

    const auto found = details_.find(ComparablePath(item->path));

    return found == details_.end() ? nullptr : &found->second;
}

const std::vector<QuarantinedItem>& QuarantineModel::Items() const
{
    return items_;
}

int QuarantineModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(items_.size());
}

int QuarantineModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : WhereColumn + 1;
}

QVariant QuarantineModel::data(const QModelIndex& position, const int role) const
{
    const QuarantinedItem* item = ItemAt(position);
    if (item == nullptr)
    {
        return {};
    }

    const QuarantineDetail* detail = DetailAt(position);
    const bool replaced = detail != nullptr && detail->WasReplaced();

    if (role == ReplacedRole)
    {
        return replaced;
    }

    if (role == TagTextRole)
    {
        return position.column() == NameColumn && replaced ? QVariant(tr("already replaced")) : QVariant();
    }

    if (role == TagToneRole)
    {
        return static_cast<int>(TagTone::Muted);
    }

    if (role == QuietRole)
    {
        return position.column() == WhereColumn || position.column() == VersionColumn;
    }

    if (role != Qt::DisplayRole)
    {
        return {};
    }

    switch (position.column())
    {
    case NameColumn: return AsText(item->path.filename());
    case VersionColumn: return detail == nullptr ? QString() : QString::fromStdString(detail->version);
    case SizeColumn:
    {
        const auto measured = bytes_.find(ComparablePath(item->path));

        return measured == bytes_.end() ? QString() : AsSize(measured->second);
    }
    case OriginColumn: return item->KnowsWhereItCameFrom() ? AsText(item->origin) : tr("(the journal does not know)");
    case WhenColumn:
        return item->quarantinedAt.has_value() ? Moment(*item->quarantinedAt) : tr("(the journal does not know)");
    case WhereColumn: return AsText(item->path.parent_path());
    default: return {};
    }
}

QVariant QuarantineModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return {};
    }

    switch (section)
    {
    case NameColumn: return tr("Name");
    case VersionColumn: return tr("Version");
    case SizeColumn: return tr("Size on disk");
    case OriginColumn: return tr("Would go back to");
    case WhenColumn: return tr("Quarantined on");
    case WhereColumn: return tr("Kept in");
    default: return {};
    }
}
