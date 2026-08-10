#ifndef FS_ORGANIZER_VIEW_PRESETS_OMITTED_DIALOG_H
#define FS_ORGANIZER_VIEW_PRESETS_OMITTED_DIALOG_H

#include <QtCore/QList>
#include <QtWidgets/QDialog>

#include "viewmodel/PresetViewModel.h"

class OmittedDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit OmittedDialog(const QList<OmittedAddon>& omitted, QWidget* parent = nullptr);
};

#endif // FS_ORGANIZER_VIEW_PRESETS_OMITTED_DIALOG_H
