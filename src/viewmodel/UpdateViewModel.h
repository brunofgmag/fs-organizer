#ifndef FS_ORGANIZER_VIEWMODEL_UPDATE_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_UPDATE_VIEW_MODEL_H

#include <string>

#include <QtCore/QObject>
#include <QtCore/QString>

#include "application/model/UpdateInfo.h"
#include "application/model/UpdateMode.h"
#include "application/ports/UpdateService.h"

enum class UpdateState : int
{
    Idle = 0,
    Checking = 1,
    UpToDate = 2,
    Available = 3,
    Downloading = 4,
    ReadyToApply = 5,
    Failed = 6,
};

class UpdateViewModel final : public QObject, public UpdateServiceObserver
{
    Q_OBJECT

public:
    UpdateViewModel(UpdateService& service,
                    QString currentVersion,
                    UpdateMode mode,
                    bool updatesAreOn,
                    QObject* parent = nullptr);

    ~UpdateViewModel() override;

    [[nodiscard]] UpdateState State() const;

    [[nodiscard]] QString WhatIsGoingOn() const;

    [[nodiscard]] QString CurrentVersion() const;

    [[nodiscard]] QString OfferedVersion() const;

    [[nodiscard]] QString ReleasePageUrl() const;

    [[nodiscard]] bool UpdatesAreOn() const;

    [[nodiscard]] bool CanCheck() const;

    [[nodiscard]] bool CanDownload() const;

    [[nodiscard]] UpdateMode Mode() const;

    [[nodiscard]] bool ShouldApplyOnExit() const;

    void ChooseMode(UpdateMode mode);

    void Check();

    void CheckQuietly();

    void Download();

    void ApplyAndRestart();

    void OnCheckFinished(bool ok, bool updateAvailable, const UpdateInfo& info, const std::string& error) override;

    void OnDownloadProgress(long long received, long long total) override;

    void OnStageFinished(bool ok, const std::string& error) override;

signals:
    void Changed();

    void ModeChosen(UpdateMode mode);

private:
    void BeginDownload();

    void SetState(UpdateState state);

    UpdateService& service_;
    QString currentVersion_;
    UpdateMode mode_;
    bool updatesAreOn_;

    UpdateState state_ = UpdateState::Idle;
    bool askedByHand_ = false;
    UpdateInfo offered_;
    int progress_ = 0;
    QString failure_;
};

#endif // FS_ORGANIZER_VIEWMODEL_UPDATE_VIEW_MODEL_H
