#ifndef FS_ORGANIZER_VIEWMODEL_ADDON_TREE_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_ADDON_TREE_VIEW_MODEL_H

#include <string>
#include <vector>

#include <QtCore/QObject>

#include "application/ProfileService.h"
#include "application/ports/SettingsRepository.h"
#include "domain/ports/ProcessProbe.h"
#include "viewmodel/AddonTreeModel.h"

class QThread;

class AddonTreeViewModel final : public QObject
{
    Q_OBJECT

public:
    AddonTreeViewModel(ProfileService& service,
                       SettingsRepository& settings,
                       const ProcessProbe& probe,
                       AddonTreeModel& model,
                       QObject* parent = nullptr);

    void ShowActiveProfile();

    void ChooseProfile(const std::string& profileId);

    void Toggle(const std::vector<const TreeNode*>& nodes);

    void Toggle(const std::vector<const TreeNode*>& nodes, bool enable);

    void UndoLastBatch();

    void OverrideDestination(const TreeNode* node, const std::filesystem::path& destination);

    [[nodiscard]] LibraryReport AddLibrary(const std::filesystem::path& path);

    [[nodiscard]] bool CanUndo() const;

    [[nodiscard]] const SimulatorProfile& Profile() const;

signals:
    void ScanStarted();

    void ScanFinished();

    void BatchFinished(const std::vector<LinkOperationResult>& results);

    void SimulatorIsRunning();

    void RestartPendingChanged(bool pending);

private:
    void StartScan();

    void AdoptScan();

    void ApplyResults(const std::vector<LinkOperationResult>& results);

    void NoteSimulatorState(const std::vector<LinkOperationResult>& results);

    void SaveProfile() const;

    ProfileService& service_;
    SettingsRepository& settings_;
    const ProcessProbe& probe_;
    AddonTreeModel& model_;
    SimulatorProfile profile_;
    ProfileSnapshot scanned_;
    QThread* scan_ = nullptr;
    bool rescanWhenIdle_ = false;
    bool warnedAboutSimulator_ = false;
    bool restartPending_ = false;
};

#endif // FS_ORGANIZER_VIEWMODEL_ADDON_TREE_VIEW_MODEL_H
