#ifndef FS_ORGANIZER_VIEWMODEL_IMPORT_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_IMPORT_VIEW_MODEL_H

#include <atomic>
#include <functional>
#include <vector>

#include <QtCore/QObject>

#include "application/ImportService.h"
#include "application/ProfileService.h"
#include "application/Session.h"
#include "application/ports/BackgroundRunner.h"

class QThread;

struct ConflictToResolve
{
    CopyConflict conflict{};
    ConflictChoice choice = ConflictChoice::KeepTheLibraryCopy;
};

class ImportViewModel final : public QObject
{
    Q_OBJECT

public:
    ImportViewModel(const ImportService& service,
                    ProfileService& profileService,
                    const ProcessProbe& probe,
                    Session& session,
                    BackgroundRunner& runner,
                    QObject* parent = nullptr);

    void Import(const std::vector<ImportRequest>& requests);

    void Resume(const std::vector<StagingLeftover>& leftovers);

    void ResolveConflicts(const std::vector<ConflictToResolve>& chosen);

    void GiveBack(const std::vector<std::filesystem::path>& addonFolders);

    void Cancel();

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

    void Idle();

    void Finished(const std::vector<ImportOperationResult>& results);

    void ConflictsResolved(const std::vector<FileOperationResult>& results);

    void GaveBack(const std::vector<FileOperationResult>& results);

private:
    [[nodiscard]] std::function<void(OperationKind)> OnStep();

    [[nodiscard]] std::function<bool(const CopyProgress&)> OnProgressOfFolder(int folder);

    void RunInAWorker(std::function<void()> work, std::function<void()> land, int folders);

    void Adopt(const std::vector<ImportOperationResult>& results);

    void AdoptWhatWentBack(const std::vector<FileOperationResult>& results);

    const ImportService& service_;
    ProfileService& profileService_;
    const ProcessProbe& probe_;
    Session& session_;
    BackgroundRunner& runner_;
    bool running_ = false;
    std::atomic<bool> cancelled_{false};
    std::atomic<int> folder_{0};
};

#endif // FS_ORGANIZER_VIEWMODEL_IMPORT_VIEW_MODEL_H
