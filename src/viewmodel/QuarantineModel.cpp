#include "viewmodel/QuarantineModel.h"

#include <utility>

#include <QtCore/QDateTime>
#include <QtCore/QTimeZone>

#include "support/PathText.h"

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
    endResetModel();
}

const QuarantinedItem* QuarantineModel::ItemAt(const QModelIndex& position) const
{
    if (!position.isValid() || position.row() < 0 || static_cast<std::size_t>(position.row()) >= items_.size())
    {
        return nullptr;
    }

    return &items_[static_cast<std::size_t>(position.row())];
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
    return parent.isValid() ? 0 : 4;
}

QVariant QuarantineModel::data(const QModelIndex& position, const int role) const
{
    const QuarantinedItem* item = ItemAt(position);
    if (item == nullptr)
    {
        return {};
    }

    if (role != Qt::DisplayRole)
    {
        return {};
    }

    switch (position.column())
    {
    case NameColumn: return AsText(item->path.filename());
    case OriginColumn: return item->KnowsWhereItCameFrom() ? AsText(item->origin) : tr("(o diário não sabe)");
    case WhenColumn: return item->quarantinedAt.has_value() ? Moment(*item->quarantinedAt) : tr("(o diário não sabe)");
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
    case NameColumn: return tr("Nome");
    case OriginColumn: return tr("Voltaria para");
    case WhenColumn: return tr("Quarentenado em");
    case WhereColumn: return tr("Guardado em");
    default: return {};
    }
}
