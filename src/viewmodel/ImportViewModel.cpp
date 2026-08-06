#include "viewmodel/ImportViewModel.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "viewmodel/FailureText.h"

ImportViewModel::ImportViewModel(const ImportService& service,
                                 ProfileService& profileService,
                                 const ProcessProbe& probe,
                                 const Session& session,
                                 BackgroundRunner& runner,
                                 QObject* parent)
    : QObject(parent),
      service_(service),
      profileService_(profileService),
      probe_(probe),
      session_(session),
      runner_(runner)
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

FileResult ImportViewModel::ResolveConflict(const CopyConflict& conflict, const ConflictChoice choice)
{
    const FileResult result = service_.ResolveConflict(Profile(), session_.Snapshot().entries, conflict, choice);

    if (Succeeded(result))
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
                    results.push_back(ImportOperationResult{.request = request, .result = FileResult::Cancelled});
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

void ImportViewModel::RunInAWorker(std::function<std::vector<ImportOperationResult>()> work, const int folders)
{
    if (running_)
    {
        return;
    }

    running_ = true;
    cancelled_ = false;
    folder_ = 0;

    emit Started(folders);

    const auto landed = std::make_shared<std::vector<ImportOperationResult>>();

    runner_.Run(
        [work = std::move(work), landed]
        {
            *landed = work();
        },
        [this, landed]
        {
            Adopt(std::move(*landed));
        });
}

void ImportViewModel::Adopt(std::vector<ImportOperationResult> results)
{
    running_ = false;

    if (std::ranges::any_of(results,
                            [](const ImportOperationResult& result)
                            {
                                return Succeeded(result.result);
                            }))
    {
        profileService_.ForgetUndo();
    }

    emit Finished(results);
}
