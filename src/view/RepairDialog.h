#ifndef FS_ORGANIZER_VIEW_REPAIR_DIALOG_H
#define FS_ORGANIZER_VIEW_REPAIR_DIALOG_H

#include <vector>

#include <QtWidgets/QDialog>

#include "domain/linking/RepairPlan.h"

class QCheckBox;
class QComboBox;

class RepairDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit RepairDialog(const std::vector<RepairCandidate>& candidates,
                          QWidget* parent = nullptr);

    [[nodiscard]] std::vector<RepairRequest> ChosenRequests() const;

private:
    struct Row
    {
        RepairCandidate candidate;
        QCheckBox* selected = nullptr;
        QComboBox* action = nullptr;
    };

    [[nodiscard]] QWidget* CreateGroup(const QString& title,
                                       const std::vector<RepairCandidate>& candidates,
                                       bool checkedByDefault,
                                       bool showOrigin);

    std::vector<Row> rows_;
};

#endif // FS_ORGANIZER_VIEW_REPAIR_DIALOG_H
