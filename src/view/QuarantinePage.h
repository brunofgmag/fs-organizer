#ifndef FS_ORGANIZER_VIEW_QUARANTINE_PAGE_H
#define FS_ORGANIZER_VIEW_QUARANTINE_PAGE_H

#include <vector>

#include <QtWidgets/QWidget>

#include "viewmodel/QuarantineViewModel.h"

class ContextPanel;
class ModelRowDetail;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTableView;

class QuarantinePage final : public QWidget
{
    Q_OBJECT

public:
    QuarantinePage(QuarantineViewModel& viewModel, QuarantineModel& model, QWidget* parent = nullptr);

signals:
    void StatusChanged(const QString& message);

    void SummaryChanged(const QString& summary);

    void AsideChanged(const QString& aside);

private:
    void ShowTheSelectedItem();

    void ShowTheSelectedBatch(const QModelIndexList& rows);

    void ShowWhatTheActionsWillTouch(const QModelIndexList& rows) const;

    [[nodiscard]] std::vector<QuarantinedItem> Selected() const;

    void OpenTheSelectedFolder() const;

    void RestoreSelected();

    void DiscardSelected();

    void EmptyTheQuarantine();

    void Report(const QString& title, const std::vector<FileOperationResult>& results);

    void UpdateSummary();

    QuarantineViewModel& viewModel_;
    QuarantineModel& model_;
    QStackedWidget* pages_ = nullptr;
    QTableView* table_ = nullptr;
    ContextPanel* panel_ = nullptr;
    ModelRowDetail* detail_ = nullptr;
    QPushButton* restore_ = nullptr;
    QPushButton* discard_ = nullptr;
    QPushButton* empty_ = nullptr;
    QPushButton* restoreFromPanel_ = nullptr;
    QPushButton* openFolder_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_QUARANTINE_PAGE_H
