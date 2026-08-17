#ifndef FS_ORGANIZER_VIEW_LIBRARY_ADDON_TREE_PAGE_H
#define FS_ORGANIZER_VIEW_LIBRARY_ADDON_TREE_PAGE_H

#include <set>
#include <string>
#include <vector>

#include <QtWidgets/QWidget>

#include "view/panels/ModelRowDetail.h"
#include "viewmodel/AddonTreeFilterModel.h"
#include "viewmodel/AddonTreeViewModel.h"
#include "viewmodel/CoverageViewModel.h"
#include "viewmodel/DeletionViewModel.h"
#include "viewmodel/AddonDocumentsViewModel.h"
#include "viewmodel/ImportViewModel.h"
#include "viewmodel/SessionNotifier.h"

class ContextPanel;
class DependencySection;
class EmptyState;
class QCheckBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QStackedWidget;
class QTreeView;

class AddonTreePage final : public QWidget
{
    Q_OBJECT

public:
    AddonTreePage(AddonTreeViewModel& viewModel,
                  DeletionViewModel& deletion,
                  ImportViewModel& importViewModel,
                  CoverageViewModel& coverage,
                  AddonDocumentsViewModel& documents,
                  AddonTreeModel& model,
                  const SessionNotifier& notifier,
                  QWidget* parent = nullptr);

    void RefreshUndoState() const;

signals:
    void StatusChanged(const QString& message);

    void SummaryChanged(const QString& summary);

    void MeterChanged(int filled, int outOf);

    void ConflictChosen(const CopyConflict& conflict);

    void DocumentationRequested(const std::string& addon);

protected:
    void changeEvent(QEvent* event) override;

private:
    void RetranslateUi() const;

    [[nodiscard]] QWidget* CreateActions();

    [[nodiscard]] QWidget* CreateInvite();

    [[nodiscard]] QWidget* CreatePanel();

    [[nodiscard]] std::vector<const TreeNode*> Chosen(const TreeNode* clicked) const;

    [[nodiscard]] const TreeNode* Current() const;

    void ShowTheSelectedAddon();

    void ShowTheSelectedBatch(const QModelIndexList& rows);

    void ShowTheFields(const QString& size) const;

    void ShowWhatTheActionsWillTouch(const QModelIndexList& rows) const;

    void MoveTheSelectedAddon();

    void OpenTheSelectedFolder() const;

    void ShowWhatTheDocumentationHolds() const;

    void DeleteTheSelectedAddons();

    void OfferToDelete(const DeletionPlan& plan);

    void OnDeleted(const std::vector<DeletionResult>& results, DeletionRoute route);

    void OnGaveBack(const std::vector<FileOperationResult>& results);

    void ToggleSelection(bool enable);

    void OnToggleRequested(const TreeNode* node);

    [[nodiscard]] bool TheUserMeantIt(const std::vector<const TreeNode*>& nodes, bool enable);

    [[nodiscard]] std::vector<TakenPlace> SwapsTheUserAgreedTo(const std::vector<const TreeNode*>& nodes, bool enable);

    void Apply(const std::vector<const TreeNode*>& nodes, bool enable);

    void TurnOffWhatTheSimulatorAlsoCovers(const std::vector<CoverageLine>& covered);

    void SayWhatTheLibraryAlreadyCovers(const std::vector<SharedAirportsLine>& shared);

    void OnTurningThemOnWasChecked(const WhatTurningThemOnFound& found);

    [[nodiscard]] std::vector<StartupLine> StartupEntriesTheUserAgreedTo(const std::vector<const TreeNode*>& nodes,
                                                                         bool enable);

    void OnBatchFinished(const LinkBatchReport& report);

    [[nodiscard]] QString NothingChangedBecause(const LinkBatchReport& report) const;

    void OnShown();

    void NoteExpansion(const QModelIndex& position, bool expanded);

    void NoteSelection();

    void NoteScrolling(int value);

    void CarryTheRememberedPaths(const std::filesystem::path& from, const std::filesystem::path& to);

    void RestoreExpansion(const QModelIndex& parent);

    [[nodiscard]] bool RestoreSelection();

    void RestoreScrolling(int value) const;

    void GatherSelection(const QModelIndex& parent, QModelIndexList& found, QModelIndex& current) const;

    void PublishSummary();

    void ShowContextMenu(const QPoint& where);

    void AddConflictAction(QMenu& menu, const QModelIndex& position);

    void AddMoveAction(QMenu& menu, const TreeNode* node);

    void AddCategoryActions(QMenu& menu, const TreeNode* node);

    void AddStrayActions(QMenu& menu, const TreeNode* node);

    void AddDestinationActions(QMenu& menu, const TreeNode* node);

    void ChooseDestination(const std::vector<const TreeNode*>& nodes, const std::filesystem::path& destination);

    [[nodiscard]] bool AskWhetherToRelink(std::size_t strayed);

    void ShowSuggestions(const TreeNode* node);

    void BrowseForLibrary();

    [[nodiscard]] bool TheRootIsWorthKeeping(const std::filesystem::path& root);

    AddonTreeViewModel& viewModel_;
    DeletionViewModel& deletion_;
    ImportViewModel& importViewModel_;
    CoverageViewModel& coverage_;
    AddonDocumentsViewModel& documents_;
    AddonTreeModel& model_;
    QStackedWidget* pages_ = nullptr;
    QTreeView* tree_ = nullptr;
    ContextPanel* panel_ = nullptr;
    ModelRowDetail* detail_ = nullptr;
    DependencySection* dependencies_ = nullptr;
    QPushButton* relink_ = nullptr;
    QPushButton* moveTo_ = nullptr;
    QPushButton* openFolder_ = nullptr;
    QPushButton* delete_ = nullptr;
    QPushButton* documentation_ = nullptr;
    AddonTreeFilterModel* filter_ = nullptr;
    QPushButton* undo_ = nullptr;
    QPushButton* enable_ = nullptr;
    QPushButton* disable_ = nullptr;
    QPushButton* rescan_ = nullptr;
    QLineEdit* search_ = nullptr;
    QCheckBox* hideEmpty_ = nullptr;
    EmptyState* invite_ = nullptr;
    QPushButton* inviteAction_ = nullptr;
    QList<ModelRowDetail::Field> fields_;
    std::set<std::string> expanded_;
    std::set<std::string> selected_;
    std::string current_;
    int scrolled_ = 0;
    bool rebuilding_ = false;
    bool shownOnce_ = false;
};

#endif // FS_ORGANIZER_VIEW_LIBRARY_ADDON_TREE_PAGE_H
