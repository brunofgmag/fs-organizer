#ifndef FS_ORGANIZER_VIEW_SHELL_LONG_OPERATION_PROGRESS_H
#define FS_ORGANIZER_VIEW_SHELL_LONG_OPERATION_PROGRESS_H

#include <QtCore/QObject>

#include "viewmodel/ImportViewModel.h"

class ImportProgressDialog;
class QWidget;

class LongOperationProgress final : public QObject
{
    Q_OBJECT

public:
    LongOperationProgress(ImportViewModel& viewModel, QWidget* over);

private:
    void Open(int folders);

    void ShowTheBytes(qulonglong copiedBytes, qulonglong totalBytes, int folder, OperationKind step);

    void ShowTheStep(OperationKind kind);

    void Close();

    ImportViewModel& viewModel_;
    QWidget* over_ = nullptr;
    ImportProgressDialog* progress_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_SHELL_LONG_OPERATION_PROGRESS_H
