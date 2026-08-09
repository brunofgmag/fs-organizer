#include "viewmodel/QuarantineModel.h"

#include <utility>

#include <QtCore/QDateTime>
#include <QtCore/QStringList>
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

    emit dataChanged(index(0, NameColumn, {}), index(last, SourceColumn, {}));
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
    return parent.isValid() ? 0 : SourceColumn + 1;
}

QString QuarantineModel::WhatTheSourcesSay(const QuarantinedItem& item) const
{
    if (item.TheSourcesDisagree())
    {
        return tr("they disagree");
    }

    if (item.TheSourcesAgree())
    {
        return tr("the record and the Journal agree");
    }

    switch (item.source)
    {
    case OriginSource::Sidecar: return tr("the record only");
    case OriginSource::Journal: return tr("the Journal only");
    case OriginSource::Unknown: return tr("neither has it");
    }

    return {};
}

QString QuarantineModel::SizeOf(const QuarantinedItem& item) const
{
    const auto measured = bytes_.find(ComparablePath(item.path));

    return measured == bytes_.end() ? QString() : AsSize(measured->second);
}

SelectionSize QuarantineModel::TallyOf(const QModelIndexList& rows) const
{
    SelectionSize size;

    for (const QModelIndex& position : rows)
    {
        const QuarantinedItem* item = ItemAt(position);
        if (item == nullptr)
        {
            continue;
        }

        ++size.selected;

        if (const auto measured = bytes_.find(ComparablePath(item->path)); measured != bytes_.end())
        {
            size.bytes += measured->second;
            ++size.measured;
        }
    }

    return size;
}

QString QuarantineModel::WhenItWasQuarantined(const QuarantinedItem& item) const
{
    return item.quarantinedAt.has_value() ? Moment(*item.quarantinedAt) : QString();
}

QString QuarantineModel::WhenAndHowBigItIs(const QuarantinedItem& item) const
{
    QStringList told;

    if (const QString when = WhenItWasQuarantined(item); !when.isEmpty())
    {
        told.append(tr("quarantined %1").arg(when));
    }

    if (const QString size = SizeOf(item); !size.isEmpty())
    {
        told.append(size);
    }

    return told.join(QStringLiteral(" · "));
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

    if (role == SecondLineRole)
    {
        return position.column() == NameColumn ? QVariant(WhenAndHowBigItIs(*item)) : QVariant();
    }

    if (role == TagTextRole)
    {
        return position.column() == SourceColumn && item->TheSourcesDisagree() ? QVariant(WhatTheSourcesSay(*item))
                                                                               : QVariant();
    }

    if (role == TagToneRole)
    {
        return static_cast<int>(TagTone::Outlined);
    }

    if (role == QuietRole)
    {
        return position.column() == VersionColumn || (position.column() == SourceColumn && !item->TheSourcesDisagree());
    }

    if (role != Qt::DisplayRole)
    {
        return {};
    }

    switch (position.column())
    {
    case NameColumn: return AsText(item->path.filename());
    case VersionColumn: return detail == nullptr ? QString() : QString::fromStdString(detail->version);
    case OriginColumn: return item->KnowsWhereItCameFrom() ? AsText(item->origin) : tr("not recorded");
    case SourceColumn: return WhatTheSourcesSay(*item);
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
    case NameColumn: return tr("Item");
    case VersionColumn: return tr("Version");
    case OriginColumn: return tr("Goes back to");
    case SourceColumn: return tr("Source");
    default: return {};
    }
}
