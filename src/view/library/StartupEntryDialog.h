#ifndef FS_ORGANIZER_VIEW_LIBRARY_STARTUP_ENTRY_DIALOG_H
#define FS_ORGANIZER_VIEW_LIBRARY_STARTUP_ENTRY_DIALOG_H

#include <vector>

#include <QtWidgets/QDialog>

#include "application/StartupReport.h"

class StartupEntryDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit StartupEntryDialog(const std::vector<StartupLine>& carried, QWidget* parent = nullptr);
};

#endif // FS_ORGANIZER_VIEW_LIBRARY_STARTUP_ENTRY_DIALOG_H
