#ifndef FS_ORGANIZER_VIEW_QUARANTINE_RESTORE_DIALOG_H
#define FS_ORGANIZER_VIEW_QUARANTINE_RESTORE_DIALOG_H

#include <vector>

#include <QtWidgets/QDialog>

#include "application/model/QuarantinedItem.h"

class RestoreDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit RestoreDialog(const std::vector<QuarantinedItem>& items, QWidget* parent = nullptr);

    [[nodiscard]] std::vector<QuarantinedItem> Restorable() const;

private:
    std::vector<QuarantinedItem> restorable_;
};

#endif // FS_ORGANIZER_VIEW_QUARANTINE_RESTORE_DIALOG_H
