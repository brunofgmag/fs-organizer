#include "infrastructure/update/AppImageUpdateService.h"

#include <utility>

namespace
{
    constexpr auto kNotAvailableYet = "Updating the AppImage in place is not available yet.";
}

AppImageUpdateService::AppImageUpdateService(QString feedUrl, QString currentVersion)
    : feed_(std::move(feedUrl),
            std::move(currentVersion),
            [this](const FeedAnswer& answer)
            {
                SayTheCheckFinished(answer);
            })
{
}

void AppImageUpdateService::CheckForUpdates()
{
    feed_.Ask();
}

void AppImageUpdateService::DownloadAndStage(const UpdateInfo&)
{
    for (UpdateServiceObserver* observer : observers_)
    {
        observer->OnStageFinished(false, kNotAvailableYet);
    }
}

void AppImageUpdateService::DiscardStaged()
{
}

bool AppImageUpdateService::HasStagedUpdate() const
{
    return false;
}

bool AppImageUpdateService::LaunchApplyHelper(bool)
{
    return false;
}

void AppImageUpdateService::AddObserver(UpdateServiceObserver* observer)
{
    observers_.push_back(observer);
}

void AppImageUpdateService::RemoveObserver(UpdateServiceObserver* observer)
{
    std::erase(observers_, observer);
}

void AppImageUpdateService::SayTheCheckFinished(const FeedAnswer& answer) const
{
    for (UpdateServiceObserver* observer : observers_)
    {
        observer->OnCheckFinished(answer.ok, answer.available, answer.info, answer.error.toStdString());
    }
}
