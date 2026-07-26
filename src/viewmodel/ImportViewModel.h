#ifndef FS_ORGANIZER_VIEWMODEL_IMPORT_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_IMPORT_VIEW_MODEL_H

#include <atomic>
#include <functional>
#include <vector>

#include <QtCore/QObject>

#include "application/ImportService.h"
#include "application/ProfileService.h"
#include "viewmodel/AddonTreeModel.h"

class QThread;

class ImportViewModel final : public QObject
{
    Q_OBJECT

public:
    ImportViewModel(const ImportService& service,
                    ProfileService& profileService,
                    const ProcessProbe& probe,
                    const AddonTreeModel& treeModel,
                    QObject* parent = nullptr);

    void Import(const std::vector<ImportRequest>& requests);

    void Resume(const std::vector<StagingLeftover>& leftovers);

    void Cancel();

    [[nodiscard]] ImportResult ResolveConflict(const CopyConflict& conflict, ConflictChoice choice);

    [[nodiscard]] ConflictDetails DetailsOf(const CopyConflict& conflict) const;

    [[nodiscard]] std::uintmax_t TotalSizeOf(const std::vector<std::filesystem::path>& folders) const;

    [[nodiscard]] std::vector<StagingLeftover> Leftovers() const;

    [[nodiscard]] std::vector<FileOperationResult>
    DiscardLeftovers(const std::vector<StagingLeftover>& leftovers) const;

    [[nodiscard]] bool SimulatorIsRunning() const;

    [[nodiscard]] std::optional<std::string> RunningSimulator() const;

    [[nodiscard]] const SimulatorProfile& Profile() const;

signals:
    void Started(int folders);

    void Progressed(qulonglong copiedBytes, qulonglong totalBytes, int folder);

    void StepChanged(const QString& step);

    void Finished(const std::vector<ImportOperationResult>& results);

    void ConflictResolved();

private:
    [[nodiscard]] std::function<void(OperationKind)> OnStep();

    void RunInAWorker(const std::function<std::vector<ImportOperationResult>()>& work, int folders);

    void Adopt();

    const ImportService& service_;
    ProfileService& profileService_;
    const ProcessProbe& probe_;
    const AddonTreeModel& treeModel_;
    QThread* worker_ = nullptr;
    std::atomic<bool> cancelled_{false};
    std::atomic<int> folder_{0};
    std::vector<ImportOperationResult> results_;
};

#endif // FS_ORGANIZER_VIEWMODEL_IMPORT_VIEW_MODEL_H
