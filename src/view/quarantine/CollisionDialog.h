#ifndef FS_ORGANIZER_VIEW_QUARANTINE_COLLISION_DIALOG_H
#define FS_ORGANIZER_VIEW_QUARANTINE_COLLISION_DIALOG_H

#include <QtWidgets/QDialog>

#include "application/model/RestorePlan.h"

class QGridLayout;
class QLabel;

class CollisionDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CollisionDialog(const RestoreCheck& check, QWidget* parent = nullptr);

    void ShowTheSizes(const TwoSides& sides);

protected:
    void showEvent(QShowEvent* event) override;

private:
    [[nodiscard]] QLabel* AddTheSide(QGridLayout& grid, int column, const QString& title, const QString& version);

    QLabel* held_ = nullptr;
    QLabel* occupant_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_QUARANTINE_COLLISION_DIALOG_H
