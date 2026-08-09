#ifndef FS_ORGANIZER_VIEW_SHELL_LONG_OPERATION_PROGRESS_H
#define FS_ORGANIZER_VIEW_SHELL_LONG_OPERATION_PROGRESS_H

#include <QtCore/QObject>
#include <QtCore/QString>

#include "viewmodel/ImportViewModel.h"

class QProgressDialog;
class QWidget;

class LongOperationProgress final : public QObject
{
    Q_OBJECT

public:
    LongOperationProgress(ImportViewModel& viewModel, QWidget* over);

private:
    void Open(int folders);

    void ShowTheBytes(qulonglong copiedBytes, qulonglong totalBytes, int folder);

    void ShowTheStep(const QString& step);

    void Close();

    ImportViewModel& viewModel_;
    QWidget* over_ = nullptr;
    QProgressDialog* progress_ = nullptr;
    int folders_ = 0;
    int folder_ = 0;
    QString step_;
};

#endif // FS_ORGANIZER_VIEW_SHELL_LONG_OPERATION_PROGRESS_H
