#ifndef FS_ORGANIZER_VIEW_COMMUNITY_PAGE_H
#define FS_ORGANIZER_VIEW_COMMUNITY_PAGE_H

#include <optional>
#include <vector>

#include <QtWidgets/QWidget>

#include "viewmodel/CommunityViewModel.h"
#include "viewmodel/ImportViewModel.h"

class QComboBox;
class QLabel;
class QProgressDialog;
class QPushButton;
class QTableView;

class CommunityPage final : public QWidget
{
    Q_OBJECT

public:
    CommunityPage(CommunityViewModel& viewModel,
                  ImportViewModel& importViewModel,
                  CommunityModel& model,
                  QWidget* parent = nullptr);

    void ResolveConflict(const CopyConflict& conflict);

signals:
    void StatusChanged(const QString& message);

private:
    [[nodiscard]] QWidget* CreateActions();

    void OnFilterChanged(int index) const;

    void StartRepair();

    void StartImport();

    void ResolveTheSelectedConflict();

    void OnRepairFinished(const std::vector<LinkOperationResult>& results);

    void OnImportStarted(int folders);

    void OnImportProgressed(qulonglong copiedBytes, qulonglong totalBytes, int folder);

    void OnImportStep(const QString& step);

    [[nodiscard]] std::optional<ImportResult> ResolveOneConflict(const CopyConflict& conflict,
                                                                 std::size_t position,
                                                                 std::size_t total);

    void OnImportFinished(const std::vector<ImportOperationResult>& results);

    [[nodiscard]] bool TheSimulatorIsInTheWay();

    void UpdateSummary();

    CommunityViewModel& viewModel_;
    ImportViewModel& importViewModel_;
    CommunityModel& model_;
    CommunityFilterModel* filter_ = nullptr;
    QTableView* table_ = nullptr;
    QComboBox* classes_ = nullptr;
    QLabel* summary_ = nullptr;
    QPushButton* repair_ = nullptr;
    QPushButton* import_ = nullptr;
    QPushButton* resolve_ = nullptr;
    QProgressDialog* progress_ = nullptr;
    int folders_ = 0;
    int folder_ = 0;
    QString step_;
};

#endif // FS_ORGANIZER_VIEW_COMMUNITY_PAGE_H
