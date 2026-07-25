#ifndef FS_ORGANIZER_VIEW_ADDON_TREE_PAGE_H
#define FS_ORGANIZER_VIEW_ADDON_TREE_PAGE_H

#include <vector>

#include <QtWidgets/QWidget>

#include "viewmodel/AddonTreeFilterModel.h"
#include "viewmodel/AddonTreeViewModel.h"

class QPushButton;
class QStackedWidget;
class QTreeView;

class AddonTreePage final : public QWidget
{
    Q_OBJECT

public:
    AddonTreePage(AddonTreeViewModel& viewModel, AddonTreeModel& model, QWidget* parent = nullptr);

    void RefreshUndoState() const;

signals:
    void StatusChanged(const QString& message);

private:
    [[nodiscard]] QWidget* CreateActions();

    [[nodiscard]] QWidget* CreateInvite();

    [[nodiscard]] std::vector<const TreeNode*> Chosen(const TreeNode* clicked) const;

    void ToggleSelection(bool enable);

    void OnToggleRequested(const TreeNode* node) const;

    void OnBatchFinished(const std::vector<LinkOperationResult>& results);

    void OnScanFinished();

    void ShowDestinationMenu(const QPoint& where);

    void BrowseForLibrary();

    AddonTreeViewModel& viewModel_;
    AddonTreeModel& model_;
    QStackedWidget* pages_ = nullptr;
    QTreeView* tree_ = nullptr;
    AddonTreeFilterModel* filter_ = nullptr;
    QPushButton* undo_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_ADDON_TREE_PAGE_H
