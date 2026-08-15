#ifndef FS_ORGANIZER_VIEW_SHELL_IMPORT_PROGRESS_DIALOG_H
#define FS_ORGANIZER_VIEW_SHELL_IMPORT_PROGRESS_DIALOG_H

#include <QtCore/QString>
#include <QtWidgets/QDialog>

#include "domain/model/OperationKind.h"

class QLabel;
class QProgressBar;
class QPushButton;

class ImportProgressDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ImportProgressDialog(int folders, QWidget* over);

    void ShowTheStep(OperationKind kind, const QString& named);

    void ShowTheBytes(qulonglong done, qulonglong total, int folder, OperationKind step);

signals:
    void Cancelled();

private:
    [[nodiscard]] QProgressBar* BarOf(OperationKind step) const;

    void ShowTheFolder(int folder);

    QLabel* folderLine_ = nullptr;
    QLabel* copyLine_ = nullptr;
    QProgressBar* copyBar_ = nullptr;
    QLabel* checkLine_ = nullptr;
    QProgressBar* checkBar_ = nullptr;
    QLabel* bytesLine_ = nullptr;
    QPushButton* cancel_ = nullptr;
    int folders_ = 0;
    int folder_ = 0;
};

#endif // FS_ORGANIZER_VIEW_SHELL_IMPORT_PROGRESS_DIALOG_H
