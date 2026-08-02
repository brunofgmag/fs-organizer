#ifndef FS_ORGANIZER_VIEWMODEL_JOURNAL_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_JOURNAL_MODEL_H

#include <vector>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QSortFilterProxyModel>

#include "domain/journal/JournalEntries.h"
#include "domain/model/SimulatorProfile.h"

class JournalModel final : public QAbstractItemModel
{
    Q_OBJECT

public:
    void Retranslated()
    {
        emit layoutAboutToBeChanged();
        emit layoutChanged();
    }

    enum Column
    {
        WhenColumn = 0,
        OperationColumn = 1,
        AddonColumn = 2,
        LibraryColumn = 3,
        SourceColumn = 4,
        TargetColumn = 5,
        OutcomeColumn = 6,
    };

    enum Role
    {
        SucceededRole = Qt::UserRole,
    };

    explicit JournalModel(QObject* parent = nullptr);

    void ShowRecords(const std::vector<OperationRecord>& records, SimulatorProfile profile);

    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent) const override;

    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;

    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] QVariant data(const QModelIndex& position, int role) const override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    [[nodiscard]] static QString KindLabel(OperationKind kind);

private:
    [[nodiscard]] const JournalEntry* EntryAt(const QModelIndex& position) const;

    [[nodiscard]] const OperationRecord* StepAt(const QModelIndex& position) const;

    [[nodiscard]] QVariant EntryColumn(const JournalEntry& entry, int column) const;

    [[nodiscard]] QVariant StepColumn(const OperationRecord& record, int column) const;

    [[nodiscard]] QString LibraryLabel(const LibraryId& libraryId) const;

    std::vector<JournalEntry> entries_;
    SimulatorProfile profile_;
};

class JournalFilterModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit JournalFilterModel(QObject* parent = nullptr);

    void Search(const QString& text);

    void ShowOnlyWhatFailed(bool only);

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString search_;
    bool failuresOnly_ = false;
};

#endif // FS_ORGANIZER_VIEWMODEL_JOURNAL_MODEL_H
