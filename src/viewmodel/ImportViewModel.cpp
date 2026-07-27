#include "viewmodel/ImportViewModel.h"

#include <algorithm>
#include <utility>

#include <QtCore/QThread>

#include "viewmodel/FailureText.h"

ImportViewModel::ImportViewModel(const ImportService& service,
                                 ProfileService& profileService,
                                 const ProcessProbe& probe,
                                 const Session& session,
                                 QObject* parent)
    : QObject(parent), service_(service), profileService_(profileService), probe_(probe), session_(session)
{
}

const SimulatorProfile& ImportViewModel::Profile() const
{
    return session_.Profile();
}

bool ImportViewModel::SimulatorIsRunning() const
{
    return probe_.SimulatorIsRunning();
}

std::optional<std::string> ImportViewModel::RunningSimulator() const
{
    return probe_.RunningSimulator();
}

ImportResult ImportViewModel::ResolveConflict(const CopyConflict& conflict, const ConflictChoice choice)
{
    const ImportResult result = service_.ResolveConflict(Profile(), session_.Snapshot().entries, conflict, choice);

    if (result == ImportResult::Completed)
    {
        profileService_.ForgetUndo();
        emit ConflictResolved();
    }

    return result;
}

ConflictDetails ImportViewModel::DetailsOf(const CopyConflict& conflict) const
{
    return service_.DetailsOf(session_.Snapshot().entries, conflict);
}

std::uintmax_t ImportViewModel::TotalSizeOf(const std::vector<std::filesystem::path>& folders) const
{
    return service_.TotalSizeOf(folders);
}

std::vector<StagingLeftover> ImportViewModel::Leftovers() const
{
    return service_.Leftovers(Profile());
}

std::vector<FileOperationResult> ImportViewModel::DiscardLeftovers(const std::vector<StagingLeftover>& leftovers) const
{
    return service_.DiscardLeftovers(Profile(), leftovers);
}

void ImportViewModel::Cancel()
{
    cancelled_ = true;
}

void ImportViewModel::Import(const std::vector<ImportRequest>& requests)
{
    const SimulatorProfile profile = Profile();

    RunInAWorker(
        [this, profile, requests]
        {
            std::vector<ImportOperationResult> results;

            for (const ImportRequest& request : requests)
            {
                if (cancelled_)
                {
                    results.push_back(ImportOperationResult{request, ImportResult::Cancelled});
                    continue;
                }

                const int folder = ++folder_;
                const auto onProgress = [this, folder](const CopyProgress& progress)
                {
                    emit Progressed(progress.copiedBytes, progress.totalBytes, folder);

                    return !cancelled_;
                };

                const std::vector<ImportOperationResult> one =
                    service_.Import(profile, {request}, onProgress, OnStep());

                results.insert(results.end(), one.begin(), one.end());
            }

            return results;
        },
        static_cast<int>(requests.size()));
}

void ImportViewModel::Resume(const std::vector<StagingLeftover>& leftovers)
{
    const SimulatorProfile profile = Profile();

    RunInAWorker(
        [this, profile, leftovers]
        {
            const auto onProgress = [this](const CopyProgress& progress)
            {
                emit Progressed(progress.copiedBytes, progress.totalBytes, folder_);

                return !cancelled_;
            };

            folder_ = 1;

            return service_.Resume(profile, leftovers, onProgress, OnStep());
        },
        static_cast<int>(leftovers.size()));
}

std::function<void(OperationKind)> ImportViewModel::OnStep()
{
    return [this](const OperationKind kind)
    {
        if (const QString step = NameOfImportStep(kind); !step.isEmpty())
        {
            emit StepChanged(step);
        }
    };
}

void ImportViewModel::RunInAWorker(const std::function<std::vector<ImportOperationResult>()>& work, const int folders)
{
    if (worker_ != nullptr)
    {
        return;
    }

    cancelled_ = false;
    folder_ = 0;
    results_.clear();

    emit Started(folders);

    worker_ = QThread::create(
        [this, work]
        {
            results_ = work();
        });

    connect(worker_, &QThread::finished, this, &ImportViewModel::Adopt);
    worker_->start();
}

void ImportViewModel::Adopt()
{
    worker_->deleteLater();
    worker_ = nullptr;

    const std::vector<ImportOperationResult> results = std::exchange(results_, {});

    if (std::ranges::any_of(results,
                            [](const ImportOperationResult& result)
                            {
                                return result.result == ImportResult::Completed;
                            }))
    {
        profileService_.ForgetUndo();
    }

    emit Finished(results);
}
