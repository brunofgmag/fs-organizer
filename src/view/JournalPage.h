#ifndef FS_ORGANIZER_VIEW_JOURNAL_PAGE_H
#define FS_ORGANIZER_VIEW_JOURNAL_PAGE_H

#include <QtWidgets/QWidget>

#include "viewmodel/JournalViewModel.h"

class QLabel;
class QTreeView;

class JournalPage final : public QWidget
{
    Q_OBJECT

public:
    JournalPage(JournalViewModel& viewModel, JournalModel& model, QWidget* parent = nullptr);

signals:
    void StatusChanged(const QString& message);

private:
    void UpdateSummary();

    JournalViewModel& viewModel_;
    JournalModel& model_;
    JournalFilterModel* filter_ = nullptr;
    QTreeView* operations_ = nullptr;
    QLabel* summary_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_JOURNAL_PAGE_H
