#ifndef FS_ORGANIZER_VIEW_COMMUNITY_CONFLICT_DIALOG_H
#define FS_ORGANIZER_VIEW_COMMUNITY_CONFLICT_DIALOG_H

#include <QtWidgets/QDialog>

#include "application/model/ConflictDetails.h"
#include "domain/model/ConflictChoice.h"

class QGroupBox;

class ConflictDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ConflictDialog(const ConflictDetails& details, QWidget* parent = nullptr);

    [[nodiscard]] ConflictChoice Choice() const;

private:
    [[nodiscard]] QGroupBox* CreateSide(const QString& title, const ConflictSide& side);

    ConflictChoice choice_ = ConflictChoice::KeepTheLibraryCopy;
};

#endif // FS_ORGANIZER_VIEW_COMMUNITY_CONFLICT_DIALOG_H
