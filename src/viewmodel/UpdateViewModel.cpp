#include "viewmodel/UpdateViewModel.h"

#include <utility>

#include <QtCore/QCoreApplication>

UpdateViewModel::UpdateViewModel(UpdateService& service,
                                 QString currentVersion,
                                 const UpdateMode mode,
                                 const bool updatesAreOn,
                                 QObject* parent)
    : QObject(parent),
      service_(service),
      currentVersion_(std::move(currentVersion)),
      mode_(mode),
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
        return tr("As atualizações estão desligadas nesta cópia: ela roda de uma árvore de build ou o "
                  "FSORG_NO_UPDATES está no ambiente.");
    }

    switch (state_)
    {
    case UpdateState::Checking: return tr("Procurando uma versão nova…");
    case UpdateState::UpToDate: return tr("A versão instalada é a mais nova.");
    case UpdateState::Available: return tr("A versão %1 está disponível.").arg(OfferedVersion());
    case UpdateState::Downloading: return tr("Baixando a versão %1: %2%").arg(OfferedVersion()).arg(progress_);
    case UpdateState::ReadyToApply:
        return tr("A versão %1 está pronta e entra ao fechar o programa.").arg(OfferedVersion());
    case UpdateState::Failed: return failure_;
    case UpdateState::Idle: break;
    }

    return tr("Nada foi verificado ainda.");
}

QString UpdateViewModel::CurrentVersion() const
{
    return currentVersion_;
}

QString UpdateViewModel::OfferedVersion() const
{
    return QString::fromStdString(offered_.version);
}

QString UpdateViewModel::ReleasePageUrl() const
{
    return QString::fromStdString(offered_.releasePageUrl);
}

bool UpdateViewModel::UpdatesAreOn() const
{
    return updatesAreOn_;
}

bool UpdateViewModel::CanCheck() const
{
    return updatesAreOn_ && state_ != UpdateState::Checking && state_ != UpdateState::Downloading;
}

bool UpdateViewModel::CanDownload() const
{
    return updatesAreOn_ && state_ == UpdateState::Available && !offered_.zipUrl.empty();
}

UpdateMode UpdateViewModel::Mode() const
{
    return mode_;
}

bool UpdateViewModel::ShouldApplyOnExit() const
{
    return mode_ != UpdateMode::Manual && service_.HasStagedUpdate();
}

void UpdateViewModel::ChooseMode(const UpdateMode mode)
{
    if (mode_ == mode)
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

    failure_ = tr("Não deu para iniciar o atualizador.");

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

    if (service_.HasStagedUpdate())
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
