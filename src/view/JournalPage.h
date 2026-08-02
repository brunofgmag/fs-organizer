#ifndef FS_ORGANIZER_VIEW_JOURNAL_PAGE_H
#define FS_ORGANIZER_VIEW_JOURNAL_PAGE_H

#include <QtWidgets/QWidget>

#include "viewmodel/JournalViewModel.h"

class ContextPanel;
class ModelRowDetail;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeView;

class JournalPage final : public QWidget
{
    Q_OBJECT

public:
    JournalPage(JournalViewModel& viewModel, JournalModel& model, QWidget* parent = nullptr);

signals:
    void SummaryChanged(const QString& summary);

    void AsideChanged(const QString& aside);

protected:
    void changeEvent(QEvent* event) override;

private:
    void ShowTheSelectedOperation() const;

    void UpdateSummary();

    void RetranslateUi();

    JournalViewModel& viewModel_;
    JournalModel& model_;
    JournalFilterModel* filter_ = nullptr;
    QTreeView* operations_ = nullptr;
    ContextPanel* panel_ = nullptr;
    ModelRowDetail* detail_ = nullptr;
    QLineEdit* search_ = nullptr;
    QCheckBox* failuresOnly_ = nullptr;
    QPushButton* reload_ = nullptr;
    QLabel* promise_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_JOURNAL_PAGE_H
