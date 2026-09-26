#ifndef FS_ORGANIZER_INFRASTRUCTURE_UPDATE_NOTICE_ONLY_UPDATE_SERVICE_H
#define FS_ORGANIZER_INFRASTRUCTURE_UPDATE_NOTICE_ONLY_UPDATE_SERVICE_H

#include <vector>

#include <QtCore/QString>

#include "application/ports/UpdateService.h"
#include "infrastructure/update/GithubReleaseFeed.h"

class NoticeOnlyUpdateService final : public UpdateService
{
public:
    NoticeOnlyUpdateService(QString feedUrl, QString currentVersion);

    void CheckForUpdates() override;

    void DownloadAndStage(const UpdateInfo& info) override;

    void DiscardStaged() override;

    [[nodiscard]] bool HasStagedUpdate() const override;

    bool LaunchApplyHelper(bool relaunch) override;

    void AddObserver(UpdateServiceObserver* observer) override;

    void RemoveObserver(UpdateServiceObserver* observer) override;

private:
    void SayTheCheckFinished(const FeedAnswer& answer) const;

    std::vector<UpdateServiceObserver*> observers_;
    GithubReleaseFeed feed_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_UPDATE_NOTICE_ONLY_UPDATE_SERVICE_H
