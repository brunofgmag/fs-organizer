#include "viewmodel/UpdateViewModel.h"

#include <utility>

#include <QtCore/QCoreApplication>

namespace
{
    UpdateMode WhatTheDeliveryAllows(const UpdateMode stored, const UpdateDelivery delivery)
    {
        if (delivery == UpdateDelivery::NoticeOnly && stored == UpdateMode::Automatic)
        {
            return UpdateMode::Notify;
        }

        return stored;
    }
}

UpdateViewModel::UpdateViewModel(UpdateService& service,
                                 const UpdateMode mode,
                                 const bool updatesAreOn,
                                 const UpdateDelivery delivery,
                                 QObject* parent)
    : QObject(parent),
      service_(service),
      delivery_(delivery),
      mode_(WhatTheDeliveryAllows(mode, delivery)),
      updatesAreOn_(updatesAreOn)
{
    service_.AddObserver(this);
}

UpdateViewModel::~UpdateViewModel()
{
    service_.RemoveObserver(this);
}

UpdateState UpdateViewModel::State() const
{
    return state_;
}

QString UpdateViewModel::WhatIsGoingOn() const
{
    if (!updatesAreOn_)
    {
        return tr("Updates are off in this copy of the program (development build or FSORG_NO_UPDATES).");
    }

    switch (state_)
    {
    case UpdateState::Checking: return tr("Looking for a new version…");
    case UpdateState::UpToDate: return tr("You have the latest version.");
    case UpdateState::Available:
        if (!UpdatesItself())
        {
            return tr("Version %1 is available. Download it again from flightsim.to.").arg(OfferedVersion());
        }

        return tr("Version %1 is available.").arg(OfferedVersion());
    case UpdateState::Downloading: return tr("Downloading version %1: %2%").arg(OfferedVersion()).arg(progress_);
    case UpdateState::ReadyToApply:
        return tr("Version %1 is ready and will be installed when you close the program.").arg(OfferedVersion());
    case UpdateState::Failed: return failure_;
    case UpdateState::Idle: break;
    }

    return tr("Not checked yet.");
}

QString UpdateViewModel::OfferedVersion() const
{
    return QString::fromStdString(offered_.version);
}

bool UpdateViewModel::UpdatesAreOn() const
{
    return updatesAreOn_;
}

bool UpdateViewModel::UpdatesItself() const
{
    return delivery_ == UpdateDelivery::SelfUpdate;
}

bool UpdateViewModel::CanCheck() const
{
    return updatesAreOn_ && state_ != UpdateState::Checking && state_ != UpdateState::Downloading;
}

bool UpdateViewModel::CanDownload() const
{
    return UpdatesItself() && updatesAreOn_ && state_ == UpdateState::Available && !offered_.zipUrl.empty();
}

UpdateMode UpdateViewModel::Mode() const
{
    return mode_;
}

bool UpdateViewModel::ShouldApplyOnExit() const
{
    return UpdatesItself() && mode_ != UpdateMode::Manual && service_.HasStagedUpdate();
}

void UpdateViewModel::ChooseMode(const UpdateMode mode)
{
    if (mode_ == mode || WhatTheDeliveryAllows(mode, delivery_) != mode)
    {
        return;
    }

    mode_ = mode;

    emit ModeChosen(mode);
    emit Changed();

    if (updatesAreOn_ && mode_ == UpdateMode::Automatic && state_ == UpdateState::Available)
    {
        BeginDownload();
    }
}

void UpdateViewModel::Check()
{
    if (!CanCheck())
    {
        return;
    }

    askedByHand_ = true;
    failure_.clear();

    SetState(UpdateState::Checking);

    service_.CheckForUpdates();
}

void UpdateViewModel::CheckQuietly()
{
    if (!CanCheck() || mode_ == UpdateMode::Manual)
    {
        return;
    }

    askedByHand_ = false;

    SetState(UpdateState::Checking);

    service_.CheckForUpdates();
}

void UpdateViewModel::Download()
{
    if (!CanDownload())
    {
        return;
    }

    BeginDownload();
}

void UpdateViewModel::ApplyAndRestart()
{
    if (service_.LaunchApplyHelper(true))
    {
        QCoreApplication::exit(0);
        return;
    }

    failure_ = tr("The updater could not be started.");

    SetState(UpdateState::Failed);
}

void UpdateViewModel::OnCheckFinished(const bool ok,
                                      const bool updateAvailable,
                                      const UpdateInfo& info,
                                      const std::string& error)
{
    const bool wasAskedByHand = std::exchange(askedByHand_, false);

    if (!ok)
    {
        if (wasAskedByHand)
        {
            failure_ = QString::fromStdString(error);
            SetState(UpdateState::Failed);
            return;
        }

        if (state_ == UpdateState::Checking)
        {
            SetState(UpdateState::Idle);
        }

        return;
    }

    offered_ = info;
    failure_.clear();

    if (UpdatesItself() && service_.HasStagedUpdate())
    {
        SetState(UpdateState::ReadyToApply);
        return;
    }

    if (!updateAvailable)
    {
        SetState(UpdateState::UpToDate);
        return;
    }

    if (mode_ == UpdateMode::Automatic)
    {
        BeginDownload();
        return;
    }

    SetState(UpdateState::Available);
}

void UpdateViewModel::OnDownloadProgress(const long long received, const long long total)
{
    progress_ = total > 0 ? static_cast<int>(received * 100 / total) : 0;

    emit Changed();
}

void UpdateViewModel::OnStageFinished(const bool ok, const std::string& error)
{
    if (!UpdatesItself())
    {
        return;
    }

    if (!ok)
    {
        failure_ = QString::fromStdString(error);
        SetState(UpdateState::Failed);
        return;
    }

    SetState(UpdateState::ReadyToApply);
}

void UpdateViewModel::BeginDownload()
{
    progress_ = 0;

    SetState(UpdateState::Downloading);

    service_.DownloadAndStage(offered_);
}

void UpdateViewModel::SetState(const UpdateState state)
{
    state_ = state;

    emit Changed();
}
