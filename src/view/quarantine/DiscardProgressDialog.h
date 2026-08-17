#ifndef FS_ORGANIZER_VIEW_QUARANTINE_DISCARD_PROGRESS_DIALOG_H
#define FS_ORGANIZER_VIEW_QUARANTINE_DISCARD_PROGRESS_DIALOG_H

#include <QtWidgets/QDialog>

class QLabel;
class QProgressBar;

class DiscardProgressDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit DiscardProgressDialog(int items, QWidget* over);

    void ShowTheItem(int discarded, int outOf);

private:
    QLabel* line_ = nullptr;
    QProgressBar* bar_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_QUARANTINE_DISCARD_PROGRESS_DIALOG_H
