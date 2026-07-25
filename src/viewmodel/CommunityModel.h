#ifndef FS_ORGANIZER_VIEWMODEL_COMMUNITY_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_COMMUNITY_MODEL_H

#include <optional>
#include <vector>

#include <QtCore/QAbstractTableModel>
#include <QtCore/QSortFilterProxyModel>

#include "domain/model/DestinationEntry.h"
#include "domain/model/SimulatorProfile.h"

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
    };

    explicit CommunityModel(QObject* parent = nullptr);

    void ShowEntries(std::vector<DestinationEntry> entries, SimulatorProfile profile);

    [[nodiscard]] const DestinationEntry* EntryAt(const QModelIndex& position) const;

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;

    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] QVariant data(const QModelIndex& position, int role) const override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    std::vector<DestinationEntry> entries_;
    SimulatorProfile profile_;
};

class CommunityFilterModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit CommunityFilterModel(QObject* parent = nullptr);

    void ShowOnly(std::optional<EntryClassification> classification);

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    std::optional<EntryClassification> classification_;
};

#endif // FS_ORGANIZER_VIEWMODEL_COMMUNITY_MODEL_H
