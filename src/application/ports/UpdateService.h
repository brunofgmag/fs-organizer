#ifndef FS_ORGANIZER_APPLICATION_PORTS_UPDATE_SERVICE_H
#define FS_ORGANIZER_APPLICATION_PORTS_UPDATE_SERVICE_H

#include <string>

#include "application/model/UpdateInfo.h"

class UpdateServiceObserver
{
public:
    virtual ~UpdateServiceObserver() = default;

    virtual void OnCheckFinished(bool ok, bool updateAvailable, const UpdateInfo& info, const std::string& error) = 0;

    virtual void OnDownloadProgress(long long received, long long total) = 0;

    virtual void OnStageFinished(bool ok, const std::string& error) = 0;
};

class UpdateService
{
public:
    virtual ~UpdateService() = default;

    virtual void CheckForUpdates() = 0;

    virtual void DownloadAndStage(const UpdateInfo& info) = 0;

    virtual void DiscardStaged() = 0;

    [[nodiscard]] virtual bool HasStagedUpdate() const = 0;

    virtual bool LaunchApplyHelper(bool relaunch) = 0;

    virtual void AddObserver(UpdateServiceObserver* observer) = 0;

    virtual void RemoveObserver(UpdateServiceObserver* observer) = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_UPDATE_SERVICE_H
