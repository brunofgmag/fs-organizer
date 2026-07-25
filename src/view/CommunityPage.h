#ifndef FS_ORGANIZER_VIEW_COMMUNITY_PAGE_H
#define FS_ORGANIZER_VIEW_COMMUNITY_PAGE_H

#include <vector>

#include <QtWidgets/QWidget>

#include "viewmodel/CommunityViewModel.h"

class QComboBox;
class QLabel;
class QPushButton;
class QTableView;

class CommunityPage final : public QWidget
{
    Q_OBJECT

public:
    CommunityPage(CommunityViewModel& viewModel, CommunityModel& model, QWidget* parent = nullptr);

signals:
    void StatusChanged(const QString& message);

private:
    void OnFilterChanged(int index) const;

    void StartRepair();

    void OnRepairFinished(const std::vector<LinkOperationResult>& results);

    void UpdateSummary() const;

    CommunityViewModel& viewModel_;
    CommunityModel& model_;
    CommunityFilterModel* filter_ = nullptr;
    QTableView* table_ = nullptr;
    QComboBox* classes_ = nullptr;
    QLabel* summary_ = nullptr;
    QPushButton* repair_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_COMMUNITY_PAGE_H
