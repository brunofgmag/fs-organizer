#include "viewmodel/JournalModel.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "domain/support/StringUtils.h"
#include "support/MomentText.h"
#include "support/PathText.h"
#include "viewmodel/FailureText.h"

namespace
{
    constexpr quintptr kNoParent = std::numeric_limits<quintptr>::max();

    QString OutcomeOf(const OperationRecord& record)
    {
        if (StepSucceeded(record))
        {
            return QObject::tr("finished");
        }

        return std::visit(
            [](const auto value)
            {
                return Explain(value);
            },
            record.outcome);
    }

    const OperationRecord& TheStepThatNamesIt(const JournalEntry& entry)
    {
        return entry.IsASwap() ? entry.Last() : entry.First();
    }
}

JournalModel::JournalModel(QObject* parent) : QAbstractItemModel(parent)
{
}

void JournalModel::ShowRecords(const std::vector<OperationRecord>& records, SimulatorProfile profile)
{
    beginResetModel();

    entries_ = GroupOperations(records);
    std::ranges::reverse(entries_);
    profile_ = std::move(profile);

    endResetModel();
}

QString JournalModel::KindLabel(const OperationKind kind)
{
    switch (kind)
    {
    case OperationKind::EnableAddon: return tr("Enable addon");
    case OperationKind::DisableAddon: return tr("Disable addon");
    case OperationKind::RemoveBrokenLink: return tr("Remove broken link");
    case OperationKind::RepointLink: return tr("Repoint link");
    case OperationKind::ImportCopyToStaging: return tr("Copy to the staging area");
    case OperationKind::ImportVerifyStaging: return tr("Check the copy");
    case OperationKind::ImportMoveIntoPlace: return tr("Put the copy in place");
    case OperationKind::ImportRemoveSource: return tr("Remove the source folder");
    case OperationKind::QuarantineFromDestination: return tr("Quarantine the destination copy");
    case OperationKind::QuarantineFromLibrary: return tr("Quarantine the library copy");
    case OperationKind::RestoreFromQuarantine: return tr("Restore from the quarantine");
    case OperationKind::DiscardFromQuarantine: return tr("Discard from the quarantine");
    case OperationKind::MoveAddon: return tr("Move addon to another category");
    case OperationKind::CreateCategory: return tr("Create category");
    case OperationKind::RenameCategory: return tr("Rename category");
    case OperationKind::RemoveCategory: return tr("Delete category");
    case OperationKind::DiscardStaging: return tr("Discard a half finished import");
    case OperationKind::RecycleFromLibrary: return tr("Delete addon to the Recycle Bin");
    case OperationKind::DeleteFromLibrary: return tr("Delete addon permanently");
    case OperationKind::LinkTheOtherProgramsFolder: return tr("Link the other program's folder into the library");
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
        return libraryId.empty() ? QString() : tr("(library removed)");
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

    return entry != nullptr && entry->HasSteps() ? static_cast<int>(entry->steps.size()) : 0;
}

int JournalModel::columnCount(const QModelIndex&) const
{
    return 7;
}

QVariant JournalModel::EntryColumn(const JournalEntry& entry, const int column) const
{
    if (!entry.HasSteps())
    {
        return StepColumn(entry.First(), column);
    }

    switch (column)
    {
    case WhenColumn: return AsMoment(entry.First().timestamp);
    case OperationColumn: return NameOfTheGroup(entry);
    case AddonColumn: return AddonsInTheGroup(entry);
    case LibraryColumn: return LibraryLabel(TheStepThatNamesIt(entry).addonId.libraryId);
    case SourceColumn: return AsText(TheStepThatNamesIt(entry).source);
    case TargetColumn: return AsText(entry.Last().target);
    case OutcomeColumn: return entry.Succeeded() ? tr("finished") : OutcomeOf(entry.WhereItStopped());
    default: return {};
    }
}

QString JournalModel::NameOfTheGroup(const JournalEntry& entry)
{
    if (entry.IsASwap())
    {
        return tr("Swap addons");
    }

    return tr("Import (%n step)", nullptr, static_cast<int>(entry.steps.size()));
}

QString JournalModel::AddonsInTheGroup(const JournalEntry& entry)
{
    if (!entry.IsASwap())
    {
        return QString::fromStdString(entry.First().addonId.folderName);
    }

    return tr("%1 out, %2 in")
        .arg(QString::fromStdString(entry.First().addonId.folderName),
             QString::fromStdString(entry.Last().addonId.folderName));
}

QVariant JournalModel::StepColumn(const OperationRecord& record, const int column) const
{
    switch (column)
    {
    case WhenColumn: return AsMoment(record.timestamp);
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
    case WhenColumn: return tr("When");
    case OperationColumn: return tr("Operation");
    case AddonColumn: return tr("Addon");
    case LibraryColumn: return tr("Library");
    case SourceColumn: return tr("Source");
    case TargetColumn: return tr("Destination");
    case OutcomeColumn: return tr("Result");
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
