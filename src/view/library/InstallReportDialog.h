#ifndef FS_ORGANIZER_VIEW_LIBRARY_INSTALL_REPORT_DIALOG_H
#define FS_ORGANIZER_VIEW_LIBRARY_INSTALL_REPORT_DIALOG_H

#include <QtWidgets/QDialog>

#include "viewmodel/InstallOffer.h"

class InstallReportDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit InstallReportDialog(const InstallReport& report, QWidget* parent = nullptr);
};

#endif // FS_ORGANIZER_VIEW_LIBRARY_INSTALL_REPORT_DIALOG_H
