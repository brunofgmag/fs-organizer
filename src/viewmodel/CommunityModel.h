#ifndef FS_ORGANIZER_VIEWMODEL_COMMUNITY_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_COMMUNITY_MODEL_H

#include <optional>
#include <vector>

#include <QtCore/QAbstractTableModel>
#include <QtCore/QSortFilterProxyModel>

#include "domain/importing/CopyConflicts.h"
#include "domain/model/DestinationEntry.h"

class CommunityModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        NameColumn = 0,
        DestinationColumn = 1,
        ClassificationColumn = 2,
        TargetColumn = 3,
    };

    enum Role
    {
        ClassificationRole = Qt::UserRole,
        ConflictRole = Qt::UserRole + 1,
    };

    explicit CommunityModel(QObject* parent = nullptr);

    void ShowEntries(const std::vector<DestinationEntry>& entries, const CopyConflicts& conflicts);

    [[nodiscard]] const DestinationEntry* EntryAt(const QModelIndex& position) const;

    [[nodiscard]] const CopyConflict* ConflictAt(const QModelIndex& position) const;

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;

    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] QVariant data(const QModelIndex& position, int role) const override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    [[nodiscard]] static QString ClassificationName(EntryClassification classification);

private:
    void ResolveTheConflictOfEachRow();

    [[nodiscard]] const CopyConflict* ConflictOnRow(int row) const;

    std::vector<DestinationEntry> entries_;
    CopyConflicts conflicts_;
    std::vector<const CopyConflict*> conflictOfRow_;
};

class CommunityFilterModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit CommunityFilterModel(QObject* parent = nullptr);

    void ShowOnly(std::optional<EntryClassification> classification);

    void ShowOnlyTheConflicted(bool only);

    void ShowOnlyWhatHolds(const QString& text);

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    [[nodiscard]] bool TheKindIsWanted(const QModelIndex& position) const;

    [[nodiscard]] bool TheTextIsWanted(int sourceRow, const QModelIndex& sourceParent) const;

    std::optional<EntryClassification> classification_;
    bool conflictedOnly_ = false;
    QString text_;
};

#endif // FS_ORGANIZER_VIEWMODEL_COMMUNITY_MODEL_H
