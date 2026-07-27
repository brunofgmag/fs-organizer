#ifndef FS_ORGANIZER_VIEWMODEL_ADDON_TREE_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_ADDON_TREE_VIEW_MODEL_H

#include <string>
#include <vector>

#include <QtCore/QObject>

#include "application/ProfileService.h"
#include "application/Session.h"
#include "domain/ports/ProcessProbe.h"
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/SessionNotifier.h"

class AddonTreeViewModel final : public QObject
{
    Q_OBJECT

public:
    AddonTreeViewModel(Session& session,
                       ProfileService& service,
                       const ProcessProbe& probe,
                       AddonTreeModel& model,
                       const SessionNotifier& notifier,
                       QObject* parent = nullptr);

    void ShowActiveProfile() const;

    void ChooseProfile(const std::string& profileId) const;

    void Toggle(const std::vector<const TreeNode*>& nodes);

    void Toggle(const std::vector<const TreeNode*>& nodes, bool enable);

    void UndoLastBatch();

    void OverrideDestination(const TreeNode* node, const std::filesystem::path& destination) const;

    [[nodiscard]] LibraryReport AddLibrary(const std::filesystem::path& path) const;

    [[nodiscard]] bool CanUndo() const;

    [[nodiscard]] const SimulatorProfile& Profile() const;

signals:
    void Shown();

    void BatchFinished(const std::vector<LinkOperationResult>& results);

    void SimulatorIsRunning();

    void RestartPendingChanged(bool pending);

private:
    void AdoptScan();

    void ApplyResults(const std::vector<LinkOperationResult>& results);

    void NoteSimulatorState(const std::vector<LinkOperationResult>& results);

    Session& session_;
    ProfileService& service_;
    const ProcessProbe& probe_;
    AddonTreeModel& model_;
    bool warnedAboutSimulator_ = false;
    bool restartPending_ = false;
};

#endif // FS_ORGANIZER_VIEWMODEL_ADDON_TREE_VIEW_MODEL_H
