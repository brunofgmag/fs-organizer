#ifndef FS_ORGANIZER_VIEW_QUARANTINE_PAGE_H
#define FS_ORGANIZER_VIEW_QUARANTINE_PAGE_H

#include <vector>

#include <QtWidgets/QWidget>

#include "viewmodel/QuarantineViewModel.h"

class QLabel;
class QPushButton;
class QTableView;

class QuarantinePage final : public QWidget
{
    Q_OBJECT

public:
    QuarantinePage(QuarantineViewModel& viewModel, QuarantineModel& model, QWidget* parent = nullptr);

signals:
    void StatusChanged(const QString& message);

private:
    [[nodiscard]] std::vector<QuarantinedItem> Selected() const;

    void RestoreSelected();

    void DiscardSelected();

    void EmptyTheQuarantine();

    void Report(const QString& title, const std::vector<FileOperationResult>& results);

    void UpdateSummary();

    QuarantineViewModel& viewModel_;
    QuarantineModel& model_;
    QTableView* table_ = nullptr;
    QLabel* summary_ = nullptr;
    QPushButton* restore_ = nullptr;
    QPushButton* discard_ = nullptr;
    QPushButton* empty_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_QUARANTINE_PAGE_H
