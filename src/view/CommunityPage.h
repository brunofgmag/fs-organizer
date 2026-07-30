#ifndef FS_ORGANIZER_VIEW_COMMUNITY_PAGE_H
#define FS_ORGANIZER_VIEW_COMMUNITY_PAGE_H

#include <optional>
#include <vector>

#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtWidgets/QWidget>

#include "viewmodel/CommunityViewModel.h"
#include "viewmodel/ImportViewModel.h"

class ContextPanel;
class ModelRowDetail;
class QLabel;
class QProgressDialog;
class QPushButton;
class QTableView;
class QToolButton;

struct ImportableSelection
{
    std::vector<std::filesystem::path> folders;
    int conflicted = 0;
    int selected = 0;
};

class CommunityPage final : public QWidget
{
    Q_OBJECT

public:
    CommunityPage(CommunityViewModel& viewModel,
                  ImportViewModel& importViewModel,
                  CommunityModel& model,
                  QWidget* parent = nullptr);

    void ResolveConflict(const CopyConflict& conflict);

    void StartRepair();

    void StartImport();

    void ResolveTheSelectedConflict();

    void FilterBy(EntryClassification classification) const;

    void FilterByConflicted() const;

    void SelectEverythingShown() const;

signals:
    void StatusChanged(const QString& message);

    void SummaryChanged(const QString& summary);

    void AsideChanged(const QString& aside);

private:
    [[nodiscard]] QWidget* CreateFilters();

    [[nodiscard]] QWidget* CreateActions();

    [[nodiscard]] QWidget* CreatePanel();

    void ApplyFilter(int filter) const;

    void ShowFilter(int filter) const;

    void ShowTheSelectedEntry();

    void ShowTheSelectedBatch(const QModelIndexList& rows);

    void ShowTheBatchFields(const QString& size) const;

    void ShowWhatTheActionsWillTouch(const QModelIndexList& rows) const;

    void OpenTheSelectedFolder() const;

    void OnRepairFinished(const std::vector<LinkOperationResult>& results);

    void OnImportStarted(int folders);

    void OnImportProgressed(qulonglong copiedBytes, qulonglong totalBytes, int folder);

    void OnImportStep(const QString& step);

    [[nodiscard]] std::optional<FileResult>
    ResolveOneConflict(const CopyConflict& conflict, std::size_t position, std::size_t total);

    void OnImportFinished(const std::vector<ImportOperationResult>& results);

    [[nodiscard]] bool TheSimulatorIsInTheWay();

    [[nodiscard]] ImportableSelection ChosenForImport() const;

    void FitTheChips();

    void UpdateSummary();

    CommunityViewModel& viewModel_;
    ImportViewModel& importViewModel_;
    CommunityModel& model_;
    CommunityFilterModel* filter_ = nullptr;
    QTableView* table_ = nullptr;
    QList<QToolButton*> chips_;
    ContextPanel* panel_ = nullptr;
    QPushButton* importOne_ = nullptr;
    QPushButton* resolveChosen_ = nullptr;
    QPushButton* openFolder_ = nullptr;
    QList<QPair<QString, QString>> counted_;
    bool batch_ = false;
    ModelRowDetail* detail_ = nullptr;
    QProgressDialog* progress_ = nullptr;
    int folders_ = 0;
    int folder_ = 0;
    QString step_;
};

#endif // FS_ORGANIZER_VIEW_COMMUNITY_PAGE_H
