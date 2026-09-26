#include "infrastructure/update/NoticeOnlyUpdateService.h"

#include <utility>

NoticeOnlyUpdateService::NoticeOnlyUpdateService(QString feedUrl, QString currentVersion)
    : feed_(std::move(feedUrl),
            std::move(currentVersion),
            [this](const FeedAnswer& answer)
            {
                SayTheCheckFinished(answer);
            })
{
}

void NoticeOnlyUpdateService::CheckForUpdates()
{
    feed_.Ask();
}

void NoticeOnlyUpdateService::DownloadAndStage(const UpdateInfo& info)
{
    static_cast<void>(info);
}

void NoticeOnlyUpdateService::DiscardStaged()
{
}

bool NoticeOnlyUpdateService::HasStagedUpdate() const
{
    return false;
}

bool NoticeOnlyUpdateService::LaunchApplyHelper(const bool relaunch)
{
    static_cast<void>(relaunch);

    return false;
}

void NoticeOnlyUpdateService::AddObserver(UpdateServiceObserver* observer)
{
    observers_.push_back(observer);
}

void NoticeOnlyUpdateService::RemoveObserver(UpdateServiceObserver* observer)
{
    std::erase(observers_, observer);
}

void NoticeOnlyUpdateService::SayTheCheckFinished(const FeedAnswer& answer) const
{
    for (UpdateServiceObserver* observer : observers_)
    {
        observer->OnCheckFinished(answer.ok, answer.available, answer.info, answer.error.toStdString());
    }
}
