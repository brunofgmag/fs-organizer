#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_UPDATE_SERVICE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_UPDATE_SERVICE_H

#include <algorithm>
#include <string>
#include <vector>

#include "application/ports/UpdateService.h"

class FakeUpdateService final : public UpdateService
{
public:
    void CheckForUpdates() override
    {
        ++checks;
    }

    void DownloadAndStage(const UpdateInfo& info) override
    {
        ++downloads;
        asked = info;
    }

    void DiscardStaged() override
    {
        staged = false;
    }

    [[nodiscard]] bool HasStagedUpdate() const override
    {
        return staged;
    }

    bool LaunchApplyHelper(const bool relaunch) override
    {
        relaunched = relaunch;
        ++helpers;

        return helperStarts;
    }

    void AddObserver(UpdateServiceObserver* observer) override
    {
        observers.push_back(observer);
    }

    void RemoveObserver(UpdateServiceObserver* observer) override
    {
        std::erase(observers, observer);
    }

    void SayTheCheckFound(const UpdateInfo& info, const bool available) const
    {
        for (UpdateServiceObserver* observer : observers)
        {
            observer->OnCheckFinished(true, available, info, {});
        }
    }

    void SayTheCheckFailed(const std::string& error) const
    {
        for (UpdateServiceObserver* observer : observers)
        {
            observer->OnCheckFinished(false, false, {}, error);
        }
    }

    void SayTheStageFinished(const bool ok, const std::string& error = {})
    {
        staged = ok;

        for (UpdateServiceObserver* observer : observers)
        {
            observer->OnStageFinished(ok, error);
        }
    }

    std::vector<UpdateServiceObserver*> observers;
    UpdateInfo asked;
    int checks = 0;
    int downloads = 0;
    int helpers = 0;
    bool relaunched = false;
    bool staged = false;
    bool helperStarts = true;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_UPDATE_SERVICE_H
