#ifndef FS_ORGANIZER_VIEWMODEL_ADDON_TREE_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_ADDON_TREE_VIEW_MODEL_H

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>

#include "application/DependencyReport.h"
#include "application/ProfileService.h"
#include "application/Session.h"
#include "application/SizeService.h"
#include "domain/ports/SimulatorPackages.h"
#include "domain/tree/CategorySuggester.h"
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/MoveTarget.h"
#include "viewmodel/SelectionSize.h"
#include "viewmodel/SessionNotifier.h"

struct WeighedSwap
{
    MeasuredFolder goesOff{};
    MeasuredFolder goesOn{};
};

class AddonTreeViewModel final : public QObject
{
    Q_OBJECT

public:
    AddonTreeViewModel(Session& session,
                       ProfileService& service,
                       AddonTreeModel& model,
                       const SimulatorPackages& packages,
                       SizeService& sizes,
                       const SessionNotifier& notifier,
                       QObject* parent = nullptr);

    void MeasureTheSelection(const std::vector<std::filesystem::path>& addonFolders);

    void WeighTheSwaps(const std::vector<TakenPlace>& swaps,
                       std::function<void(const std::vector<WeighedSwap>&)> onWeighed);

    void ShowActiveProfile() const;

    void ChooseProfile(const std::string& profileId) const;

    void Toggle(const std::vector<const TreeNode*>& nodes);

    void Toggle(const std::vector<const TreeNode*>& nodes, bool enable);

    void Toggle(const std::vector<const TreeNode*>& nodes, bool enable, const std::vector<TakenPlace>& agreedSwaps);

    void Toggle(const std::vector<const TreeNode*>& nodes,
                bool enable,
                const std::vector<TakenPlace>& agreedSwaps,
                const std::vector<StartupLine>& agreedEntries);

    [[nodiscard]] std::vector<TakenPlace> SwapsNeededTo(const std::vector<const TreeNode*>& nodes) const;

    [[nodiscard]] std::vector<StartupLine> StartupEntriesAtRisk(const std::vector<const TreeNode*>& nodes) const;

    [[nodiscard]] QString VersionOf(const std::filesystem::path& addonFolder) const;

    [[nodiscard]] bool WouldEnable(const std::vector<const TreeNode*>& nodes) const;

    [[nodiscard]] std::size_t AddonsThatWouldChange(const std::vector<const TreeNode*>& nodes, bool enable) const;

    void UndoLastBatch();

    void OverrideDestination(const std::vector<const TreeNode*>& nodes, const std::filesystem::path& destination) const;

    void CreateCategory(const TreeNode* node, const QString& name);

    [[nodiscard]] std::filesystem::path RenameCategory(const TreeNode* node, const QString& name);

    void RemoveCategory(const TreeNode* node);

    [[nodiscard]] static bool CanRemoveCategory(const TreeNode* node);

    void MoveTo(const std::vector<const TreeNode*>& nodes, const std::filesystem::path& category);

    void ApplySuggestions(const std::vector<CategorySuggestion>& chosen);

    void AdoptDestination(const TreeNode* category);

    void RelinkToTheProfileDestination(const std::vector<const TreeNode*>& nodes);

    [[nodiscard]] std::size_t StrayAddonsUnder(const std::vector<const TreeNode*>& nodes) const;

    [[nodiscard]] std::vector<MoveTarget> CategoriesFor(const TreeNode* node) const;

    [[nodiscard]] std::vector<CategorySuggestion> SuggestionsFor(const TreeNode* node) const;

    [[nodiscard]] DependencyReport DependenciesOf(const TreeNode* node) const;

    [[nodiscard]] LibraryReport AddLibrary(const std::filesystem::path& path) const;

    [[nodiscard]] bool CanUndo() const;

    [[nodiscard]] const SimulatorProfile& Profile() const;

signals:
    void Shown();

    void BatchFinished(const LinkBatchReport& report);

    void Refused(const QString& explanation);

    void SizeMeasuring();

    void SizeMeasured(const SelectionSize& size);

private:
    [[nodiscard]] const TreeNode* LibraryTreeHolding(const TreeNode& node) const;

    [[nodiscard]] std::vector<const TreeNode*> StrayedUnder(const std::vector<const TreeNode*>& nodes) const;

    void Perform(const std::vector<AddonMove>& moves);

    void AdoptScan();

    void ApplyResults(const LinkBatchReport& report);

    Session& session_;
    ProfileService& service_;
    AddonTreeModel& model_;
    const SimulatorPackages& packages_;
    SizeService& sizes_;
    MeasurementCaller caller_;
};

#endif // FS_ORGANIZER_VIEWMODEL_ADDON_TREE_VIEW_MODEL_H
