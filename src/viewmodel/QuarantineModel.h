#ifndef FS_ORGANIZER_VIEWMODEL_QUARANTINE_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_QUARANTINE_MODEL_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <QtCore/QAbstractTableModel>

#include "application/model/QuarantinedItem.h"
#include "application/model/SizeReport.h"

class QuarantineModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        NameColumn = 0,
        VersionColumn = 1,
        SizeColumn = 2,
        OriginColumn = 3,
        WhenColumn = 4,
        WhereColumn = 5,
    };

    enum Role
    {
        ReplacedRole = Qt::UserRole,
    };

    explicit QuarantineModel(QObject* parent = nullptr);

    void ShowItems(std::vector<QuarantinedItem> items);

    void ShowDetails(const std::vector<QuarantineDetail>& details);

    void ShowSizes(const std::vector<MeasuredFolder>& sizes);

    [[nodiscard]] const QuarantinedItem* ItemAt(const QModelIndex& position) const;

    [[nodiscard]] const QuarantineDetail* DetailAt(const QModelIndex& position) const;

    [[nodiscard]] const std::vector<QuarantinedItem>& Items() const;

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;

    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] QVariant data(const QModelIndex& position, int role) const override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    void RepaintTheRows();

    std::vector<QuarantinedItem> items_;
    std::map<std::string, QuarantineDetail> details_;
    std::map<std::string, std::uintmax_t> bytes_;
};

#endif // FS_ORGANIZER_VIEWMODEL_QUARANTINE_MODEL_H
