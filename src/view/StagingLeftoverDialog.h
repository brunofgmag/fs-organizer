#ifndef FS_ORGANIZER_VIEW_STAGING_LEFTOVER_DIALOG_H
#define FS_ORGANIZER_VIEW_STAGING_LEFTOVER_DIALOG_H

#include <vector>

#include <QtWidgets/QDialog>

#include "application/model/StagingLeftover.h"

class QComboBox;

class StagingLeftoverDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit StagingLeftoverDialog(const std::vector<StagingLeftover>& leftovers, QWidget* parent = nullptr);

    [[nodiscard]] std::vector<StagingLeftover> ToResume() const;

    [[nodiscard]] std::vector<StagingLeftover> ToDiscard() const;

private:
    enum Action
    {
        LeaveItThere = 0,
        Resume = 1,
        Discard = 2,
    };

    [[nodiscard]] std::vector<StagingLeftover> Chosen(Action action) const;

    struct Row
    {
        StagingLeftover leftover;
        QComboBox* action = nullptr;
    };

    std::vector<Row> rows_;
};

#endif // FS_ORGANIZER_VIEW_STAGING_LEFTOVER_DIALOG_H
