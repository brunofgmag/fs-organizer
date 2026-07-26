#include "viewmodel/JournalModel.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <QtCore/QDateTime>
#include <QtCore/QTimeZone>

#include "support/PathText.h"
#include "viewmodel/FailureText.h"

namespace
{
    constexpr quintptr kNoParent = std::numeric_limits<quintptr>::max();

    QString Moment(const std::chrono::system_clock::time_point& timestamp)
    {
        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();

        return QDateTime::fromMSecsSinceEpoch(milliseconds, QTimeZone::UTC)
            .toLocalTime()
            .toString(QStringLiteral("dd/MM/yyyy HH:mm:ss"));
    }

    QString OutcomeOf(const OperationRecord& record)
    {
        if (StepSucceeded(record))
        {
            return QObject::tr("concluída");
        }

        return std::visit(
            [](const auto value)
            {
                return Explain(value);
            },
            record.outcome);
    }
}

JournalModel::JournalModel(QObject* parent) : QAbstractItemModel(parent)
{
}

void JournalModel::ShowRecords(const std::vector<OperationRecord>& records, SimulatorProfile profile)
{
    beginResetModel();

    entries_ = GroupImportRuns(records);
    std::ranges::reverse(entries_);
    profile_ = std::move(profile);

    endResetModel();
}

QString JournalModel::KindLabel(const OperationKind kind)
{
    switch (kind)
    {
    case OperationKind::EnableAddon: return tr("Habilitar addon");
    case OperationKind::DisableAddon: return tr("Desabilitar addon");
    case OperationKind::RemoveBrokenLink: return tr("Remover link quebrado");
    case OperationKind::RepointLink: return tr("Re-apontar link");
    case OperationKind::ImportCopyToStaging: return tr("Copiar para a área de staging");
    case OperationKind::ImportVerifyStaging: return tr("Verificar a cópia");
    case OperationKind::ImportMoveIntoPlace: return tr("Pôr a cópia no lugar");
    case OperationKind::ImportRemoveSource: return tr("Remover a pasta de origem");
    case OperationKind::QuarantineFromDestination: return tr("Quarentenar a cópia do destino");
    case OperationKind::QuarantineFromLibrary: return tr("Quarentenar a cópia da biblioteca");
    case OperationKind::RestoreFromQuarantine: return tr("Restaurar da quarentena");
    case OperationKind::DiscardFromQuarantine: return tr("Descartar da quarentena");
    case OperationKind::MoveAddon: return tr("Mover addon de categoria");
    case OperationKind::CreateCategory: return tr("Criar categoria");
    case OperationKind::RenameCategory: return tr("Renomear categoria");
    case OperationKind::DiscardStaging: return tr("Descartar uma importação pela metade");
    }

    return {};
}

QString JournalModel::LibraryLabel(const LibraryId& libraryId) const
{
    const auto library = std::ranges::find_if(profile_.libraries,
                                              [&libraryId](const Library& candidate)
                                              {
                                                  return EqualsIgnoringCase(candidate.id, libraryId);
                                              });

    if (library == profile_.libraries.end())
    {
        return libraryId.empty() ? QString() : tr("(biblioteca removida)");
    }

    return library->label.empty() ? AsText(library->path.filename()) : QString::fromStdString(library->label);
}

const JournalEntry* JournalModel::EntryAt(const QModelIndex& position) const
{
    if (!position.isValid() || position.internalId() != kNoParent)
    {
        return nullptr;
    }

    return &entries_[static_cast<std::size_t>(position.row())];
}

const OperationRecord* JournalModel::StepAt(const QModelIndex& position) const
{
    if (!position.isValid() || position.internalId() == kNoParent)
    {
        return nullptr;
    }

    return &entries_[static_cast<std::size_t>(position.internalId())].steps[static_cast<std::size_t>(position.row())];
}

QModelIndex JournalModel::index(const int row, const int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
    {
        return {};
    }

    return parent.isValid() ? createIndex(row, column, static_cast<quintptr>(parent.row()))
                            : createIndex(row, column, kNoParent);
}

QModelIndex JournalModel::parent(const QModelIndex& child) const
{
    if (!child.isValid() || child.internalId() == kNoParent)
    {
        return {};
    }

    return createIndex(static_cast<int>(child.internalId()), 0, kNoParent);
}

int JournalModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return static_cast<int>(entries_.size());
    }

    if (parent.column() > 0)
    {
        return 0;
    }

    const JournalEntry* entry = EntryAt(parent);

    return entry != nullptr && entry->IsAnImportRun() ? static_cast<int>(entry->steps.size()) : 0;
}

int JournalModel::columnCount(const QModelIndex&) const
{
    return 7;
}

QVariant JournalModel::EntryColumn(const JournalEntry& entry, const int column) const
{
    if (!entry.IsAnImportRun())
    {
        return StepColumn(entry.First(), column);
    }

    switch (column)
    {
    case WhenColumn: return Moment(entry.First().timestamp);
    case OperationColumn: return tr("Importação (%n passo(s))", nullptr, static_cast<int>(entry.steps.size()));
    case AddonColumn: return QString::fromStdString(entry.First().addonId.folderName);
    case LibraryColumn: return LibraryLabel(entry.First().addonId.libraryId);
    case SourceColumn: return AsText(entry.First().source);
    case TargetColumn: return AsText(entry.Last().target);
    case OutcomeColumn: return entry.Succeeded() ? tr("concluída") : OutcomeOf(entry.Last());
    default: return {};
    }
}

QVariant JournalModel::StepColumn(const OperationRecord& record, const int column) const
{
    switch (column)
    {
    case WhenColumn: return Moment(record.timestamp);
    case OperationColumn: return KindLabel(record.kind);
    case AddonColumn: return QString::fromStdString(record.addonId.folderName);
    case LibraryColumn: return LibraryLabel(record.addonId.libraryId);
    case SourceColumn: return AsText(record.source);
    case TargetColumn: return AsText(record.target);
    case OutcomeColumn: return OutcomeOf(record);
    default: return {};
    }
}

QVariant JournalModel::data(const QModelIndex& position, const int role) const
{
    if (role == SucceededRole)
    {
        if (const JournalEntry* entry = EntryAt(position))
        {
            return entry->Succeeded();
        }

        const OperationRecord* step = StepAt(position);

        return step != nullptr && StepSucceeded(*step);
    }

    if (role != Qt::DisplayRole)
    {
        return {};
    }

    if (const JournalEntry* entry = EntryAt(position))
    {
        return EntryColumn(*entry, position.column());
    }

    if (const OperationRecord* step = StepAt(position))
    {
        return StepColumn(*step, position.column());
    }

    return {};
}

QVariant JournalModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return {};
    }

    switch (section)
    {
    case WhenColumn: return tr("Quando");
    case OperationColumn: return tr("Operação");
    case AddonColumn: return tr("Addon");
    case LibraryColumn: return tr("Biblioteca");
    case SourceColumn: return tr("Origem");
    case TargetColumn: return tr("Destino");
    case OutcomeColumn: return tr("Resultado");
    default: return {};
    }
}

JournalFilterModel::JournalFilterModel(QObject* parent) : QSortFilterProxyModel(parent)
{
    setRecursiveFilteringEnabled(true);
}

void JournalFilterModel::Search(const QString& text)
{
    search_ = text.trimmed();
    invalidateRowsFilter();
}

void JournalFilterModel::ShowOnlyWhatFailed(const bool only)
{
    failuresOnly_ = only;
    invalidateRowsFilter();
}

bool JournalFilterModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex position = sourceModel()->index(sourceRow, 0, sourceParent);

    if (failuresOnly_ && sourceModel()->data(position, JournalModel::SucceededRole).toBool())
    {
        return false;
    }

    if (search_.isEmpty())
    {
        return true;
    }

    for (int column = 0; column < sourceModel()->columnCount(sourceParent); ++column)
    {
        if (sourceModel()
                ->data(position.siblingAtColumn(column), Qt::DisplayRole)
                .toString()
                .contains(search_, Qt::CaseInsensitive))
        {
            return true;
        }
    }

    return false;
}
