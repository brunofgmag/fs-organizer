#ifndef FS_ORGANIZER_VIEW_QUARANTINE_QUARANTINE_PAGE_H
#define FS_ORGANIZER_VIEW_QUARANTINE_QUARANTINE_PAGE_H

#include <vector>

#include <QtWidgets/QWidget>

#include "view/panels/ModelRowDetail.h"
#include "viewmodel/QuarantineViewModel.h"

class ContextPanel;
class DiscardProgressDialog;
class EmptyState;
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

protected:
    void changeEvent(QEvent* event) override;

private:
    void RetranslateUi();

    void ShowTheSelectedItem();

    void ShowTheSelectedBatch(const QModelIndexList& rows) const;

    [[nodiscard]] QList<ModelRowDetail::Field> WhatTheTableDoesNotShow(const QModelIndex& position) const;

    [[nodiscard]] static QList<ModelRowDetail::Field> WhereEachSourcePoints(const QuarantinedItem& item);

    [[nodiscard]] QList<ModelRowDetail::Field> TheComparisonFor(const QModelIndex& position) const;

    void ShowWhatTheActionsWillTouch(const QModelIndexList& rows) const;

    [[nodiscard]] std::vector<QuarantinedItem> Selected() const;

    void OpenTheSelectedFolder() const;

    void RestoreSelected();

    void OfferTheRestore(const std::vector<RestoreOffer>& offers);

    void DiscardSelected();

    void EmptyTheQuarantine();

    void OpenTheProgress(int items);

    void CloseTheProgress();

    void Report(const QString& title, const std::vector<FileOperationResult>& results);

    [[nodiscard]] bool AskAboutTheCollision(const RestoreCheck& check);

    void ReportTheSwaps(const std::vector<SwapResult>& results);

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
    EmptyState* nothingHeld_ = nullptr;
    DiscardProgressDialog* progress_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_QUARANTINE_QUARANTINE_PAGE_H
