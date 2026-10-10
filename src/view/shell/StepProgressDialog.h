#ifndef FS_ORGANIZER_VIEW_SHELL_STEP_PROGRESS_DIALOG_H
#define FS_ORGANIZER_VIEW_SHELL_STEP_PROGRESS_DIALOG_H

#include <vector>

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtWidgets/QDialog>

class QLabel;
class QProgressBar;
class QPushButton;

class StepProgressDialog final : public QDialog
{
    Q_OBJECT

public:
    StepProgressDialog(const QString& title, const QStringList& steps, QWidget* over);

    void NameTheSteps(const QStringList& steps);

    void ShowTheSubject(const QString& subject);

    void ShowTheStep(int step);

    void ShowTheProgress(qulonglong done, qulonglong total);

    void ShowThatItCannotBeMeasured();

    void ShowTheDetail(const QString& detail);

    void ShowItIsCancelling(const QString& detail);

signals:
    void Cancelled();

private:
    void ShowCutInTheMiddle(QLabel& line, const QString& said) const;

    QLabel* subject_ = nullptr;
    std::vector<QLabel*> names_;
    std::vector<QProgressBar*> bars_;
    QLabel* detail_ = nullptr;
    QPushButton* cancel_ = nullptr;
    int step_ = 0;
};

#endif // FS_ORGANIZER_VIEW_SHELL_STEP_PROGRESS_DIALOG_H
