#include "viewmodel/ImportViewModel.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "viewmodel/FailureText.h"

ImportViewModel::ImportViewModel(const ImportService& service,
                                 ProfileService& profileService,
                                 const ProcessProbe& probe,
                                 Session& session,
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
    const auto landed = std::make_shared<std::vector<ImportOperationResult>>();

    RunInAWorker(
        [this, profile, requests, landed]
        {
            for (const ImportRequest& request : requests)
            {
                if (cancelled_)
                {
                    landed->push_back(ImportOperationResult{.request = request, .result = FileResult::Cancelled});
                    continue;
                }

                const std::vector<ImportOperationResult> one =
                    service_.Import(profile, {request}, OnProgressOfFolder(++folder_), OnStep());

                landed->insert(landed->end(), one.begin(), one.end());
            }
        },
        [this, landed]
        {
            Adopt(*landed);
        },
        static_cast<int>(requests.size()));
}

void ImportViewModel::Resume(const std::vector<StagingLeftover>& leftovers)
{
    const SimulatorProfile profile = Profile();
    const auto landed = std::make_shared<std::vector<ImportOperationResult>>();

    RunInAWorker(
        [this, profile, leftovers, landed]
        {
            folder_ = 1;

            *landed = service_.Resume(profile, leftovers, OnProgressOfFolder(folder_), OnStep());
        },
        [this, landed]
        {
            Adopt(*landed);
        },
        static_cast<int>(leftovers.size()));
}

void ImportViewModel::ResolveConflicts(const std::vector<ConflictToResolve>& chosen)
{
    const SimulatorProfile profile = Profile();
    const std::vector<DestinationEntry> entries = session_.Snapshot().entries;
    const auto landed = std::make_shared<std::vector<FileOperationResult>>();

    RunInAWorker(
        [this, profile, entries, chosen, landed]
        {
            for (const ConflictToResolve& one : chosen)
            {
                const FileResult result = cancelled_
                    ? FileResult::Cancelled
                    : service_.ResolveConflict(profile, entries, one.conflict, one.choice,
                                               OnProgressOfFolder(++folder_), OnStep());

                landed->push_back(FileOperationResult{.path = one.conflict.provenancePath, .result = result});
            }
        },
        [this, landed]
        {
            if (std::ranges::any_of(*landed,
                                    [](const FileOperationResult& result)
                                    {
                                        return Succeeded(result.result);
                                    }))
            {
                profileService_.ForgetUndo();
            }

            emit ConflictsResolved(*landed);
        },
        static_cast<int>(chosen.size()));
}

void ImportViewModel::GiveBack(const std::vector<std::filesystem::path>& addonFolders)
{
    const SimulatorProfile profile = Profile();
    const std::vector<DestinationEntry> entries = session_.Snapshot().entries;
    const auto landed = std::make_shared<std::vector<FileOperationResult>>();

    RunInAWorker(
        [this, profile, entries, addonFolders, landed]
        {
            for (const std::filesystem::path& addonFolder : addonFolders)
            {
                const FileResult result = cancelled_
                    ? FileResult::Cancelled
                    : service_.GiveBack(profile, entries, addonFolder, OnProgressOfFolder(++folder_), OnStep());

                landed->push_back(FileOperationResult{.path = addonFolder, .result = result});
            }
        },
        [this, landed]
        {
            AdoptWhatWentBack(*landed);
        },
        static_cast<int>(addonFolders.size()));
}

std::function<bool(const CopyProgress&)> ImportViewModel::OnProgressOfFolder(const int folder)
{
    return [this, folder](const CopyProgress& progress)
    {
        emit Progressed(progress.copiedBytes, progress.totalBytes, folder);

        return !cancelled_;
    };
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

void ImportViewModel::RunInAWorker(std::function<void()> work, std::function<void()> land, const int folders)
{
    if (running_)
    {
        return;
    }

    running_ = true;
    cancelled_ = false;
    folder_ = 0;

    emit Started(folders);

    runner_.Run(std::move(work),
                [this, land = std::move(land)]
                {
                    running_ = false;

                    emit Idle();

                    land();
                });
}

void ImportViewModel::Adopt(const std::vector<ImportOperationResult>& results)
{
    session_.RememberWhatCameFromAnotherProgram(results);

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

void ImportViewModel::AdoptWhatWentBack(const std::vector<FileOperationResult>& results)
{
    std::vector<std::filesystem::path> returned;

    for (const FileOperationResult& result : results)
    {
        if (Succeeded(result.result))
        {
            returned.push_back(result.path);
        }
    }

    session_.ForgetWhatCameFromAnotherProgram(returned);

    if (!returned.empty())
    {
        profileService_.ForgetUndo();
    }

    emit GaveBack(results);
}
