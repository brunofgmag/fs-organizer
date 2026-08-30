#ifndef FS_ORGANIZER_VIEW_COMMUNITY_COMMUNITY_PAGE_H
#define FS_ORGANIZER_VIEW_COMMUNITY_COMMUNITY_PAGE_H

#include <optional>
#include <vector>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QSet>
#include <QtWidgets/QWidget>

#include "viewmodel/CommunityViewModel.h"
#include "viewmodel/ImportViewModel.h"

class ContextPanel;
class ModelRowDetail;
class QLabel;
class QPushButton;
class QTableView;
class QToolButton;

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

protected:
    void changeEvent(QEvent* event) override;

private:
    void RetranslateUi();

    struct FilterChip
    {
        QString label;
        int filter;
    };

    [[nodiscard]] static QList<FilterChip> FiltersOffered();

    [[nodiscard]] QWidget* CreateFilters();

    [[nodiscard]] QWidget* CreateActions();

    [[nodiscard]] QWidget* CreatePanel();

    void ApplyFilter(int filter) const;

    void ShowFilter(int filter) const;

    void ShowTheSelectedEntry();

    struct Tally
    {
        QHash<int, int> counted;
        QSet<QString> destinations;
        bool alarming = false;
    };

    [[nodiscard]] Tally TallyOf(const QModelIndexList& rows) const;

    void ShowTheSelectedBatch(const QModelIndexList& rows);

    [[nodiscard]] std::vector<DestinationEntry> SelectedEntries(const QModelIndexList& rows) const;

    void ShowTheBatchFields(const QString& size) const;

    void ShowWhatTheActionsWillTouch(const QModelIndexList& rows) const;

    void OpenTheSelectedFolder() const;

    void OnRepairFinished(const std::vector<LinkOperationResult>& results);

    void ResolveThem(const std::vector<CopyConflict>& conflicts);

    void OfferTheResolutions(const std::vector<CopyConflict>& conflicts, const std::vector<ConflictDetails>& details);

    void OnConflictsResolved(const std::vector<FileOperationResult>& results);

    void OnImportFinished(const std::vector<ImportOperationResult>& results);

    [[nodiscard]] bool TheSimulatorIsInTheWay();

    void FitTheChips();

    void LeaveAFilterThatRanOut(const QHash<int, int>& counted);

    void UpdateSummary();

    CommunityViewModel& viewModel_;
    ImportViewModel& importViewModel_;
    CommunityModel& model_;
    CommunityFilterModel* filter_ = nullptr;
    QTableView* table_ = nullptr;
    QList<QToolButton*> chips_;
    ContextPanel* panel_ = nullptr;
    QPushButton* selectAll_ = nullptr;
    QPushButton* reread_ = nullptr;
    QLineEdit* search_ = nullptr;
    QLabel* promise_ = nullptr;
    QPushButton* importOne_ = nullptr;
    QPushButton* resolveChosen_ = nullptr;
    QPushButton* openFolder_ = nullptr;
    QList<QPair<QString, QString>> counted_;
    bool batch_ = false;
    ModelRowDetail* detail_ = nullptr;
    bool everyConflictWasAsked_ = true;
};

#endif // FS_ORGANIZER_VIEW_COMMUNITY_COMMUNITY_PAGE_H
