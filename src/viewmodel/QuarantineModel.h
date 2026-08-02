#ifndef FS_ORGANIZER_VIEWMODEL_QUARANTINE_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_QUARANTINE_MODEL_H

#include <vector>

#include <QtCore/QAbstractTableModel>

#include "application/model/QuarantinedItem.h"

class QuarantineModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        NameColumn = 0,
        OriginColumn = 1,
        WhenColumn = 2,
        WhereColumn = 3,
    };

    explicit QuarantineModel(QObject* parent = nullptr);

    void ShowItems(std::vector<QuarantinedItem> items);

    [[nodiscard]] const QuarantinedItem* ItemAt(const QModelIndex& position) const;

    [[nodiscard]] const std::vector<QuarantinedItem>& Items() const;

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;

    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] QVariant data(const QModelIndex& position, int role) const override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    std::vector<QuarantinedItem> items_;
};

#endif // FS_ORGANIZER_VIEWMODEL_QUARANTINE_MODEL_H
