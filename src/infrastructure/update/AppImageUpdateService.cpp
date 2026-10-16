#include "infrastructure/update/AppImageUpdateService.h"

#include <cstdio>
#include <utility>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtNetwork/QNetworkReply>

#include "infrastructure/update/GithubReleaseParser.h"

namespace
{
    constexpr int kTransferTimeoutMs = 30000;

    constexpr auto kStagedName = ".fs-organizer-update.AppImage";
    constexpr auto kPartSuffix = ".part";

    constexpr auto kWaitThenRun = "while kill -0 \"$1\" 2>/dev/null; do sleep 0.2; done; exec \"$2\"";

    constexpr QFileDevice::Permissions kRunnable = QFileDevice::ReadOwner | QFileDevice::WriteOwner
        | QFileDevice::ExeOwner | QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther
        | QFileDevice::ExeOther;

    QNetworkReply* Taken(QNetworkReply*& member)
    {
        QNetworkReply* reply = std::exchange(member, nullptr);
        reply->deleteLater();

        return reply;
    }

    bool Renamed(const QString& from, const QString& to)
    {
        return std::rename(QFile::encodeName(from).constData(), QFile::encodeName(to).constData()) == 0;
    }
}

AppImageUpdateService::AppImageUpdateService(QString feedUrl,
                                             QString currentVersion,
                                             QString appImagePath,
                                             const qint64 ownerPid,
                                             QObject* parent)
    : QObject(parent),
      currentVersion_(std::move(currentVersion)),
      appImagePath_(std::move(appImagePath)),
      ownerPid_(ownerPid),
      feed_(std::move(feedUrl),
            currentVersion_,
            [this](const FeedAnswer& answer)
            {
                OnCheckFinished(answer);
            })
{
    network_.setTransferTimeout(kTransferTimeoutMs);
}

void AppImageUpdateService::CheckForUpdates()
{
    feed_.Ask();
}

void AppImageUpdateService::DownloadAndStage(const UpdateInfo& info)
{
    if (checksumReply_ != nullptr || imageReply_ != nullptr)
    {
        return;
    }

    if (info.appImageUrl.empty() || info.appImageShaUrl.empty())
    {
        SayTheStageFinished(false, tr("This release has no files to download."));
        return;
    }

    DiscardStaged();
    pending_ = info;

    checksumReply_ = StartGet(QString::fromStdString(info.appImageShaUrl));

    connect(checksumReply_, &QNetworkReply::finished, this, &AppImageUpdateService::OnChecksumFinished);
}

void AppImageUpdateService::DiscardStaged()
{
    if (checksumReply_ != nullptr)
    {
        checksumReply_->abort();
    }

    if (imageReply_ != nullptr)
    {
        imageReply_->abort();
    }

    QFile::remove(PartPath());
    QFile::remove(StagedPath());

    stagedVersion_.clear();
}

bool AppImageUpdateService::HasStagedUpdate() const
{
    return !stagedVersion_.isEmpty() && QFile::exists(StagedPath());
}

bool AppImageUpdateService::LaunchApplyHelper(const bool relaunch)
{
    if (applied_)
    {
        return true;
    }

    if (!HasStagedUpdate() || !Renamed(StagedPath(), appImagePath_))
    {
        return false;
    }

    applied_ = true;
    stagedVersion_.clear();

    return !relaunch || Relaunch();
}

bool AppImageUpdateService::Relaunch() const
{
    return QProcess::startDetached(QStringLiteral("/bin/sh"),
                                   {QStringLiteral("-c"), QString::fromLatin1(kWaitThenRun),
                                    QStringLiteral("fs-organizer"), QString::number(ownerPid_), appImagePath_});
}

void AppImageUpdateService::AddObserver(UpdateServiceObserver* observer)
{
    observers_.push_back(observer);
}

void AppImageUpdateService::RemoveObserver(UpdateServiceObserver* observer)
{
    std::erase(observers_, observer);
}

void AppImageUpdateService::OnCheckFinished(const FeedAnswer& answer)
{
    if (!answer.ok)
    {
        for (UpdateServiceObserver* observer : observers_)
        {
            observer->OnCheckFinished(false, false, {}, answer.error.toStdString());
        }

        return;
    }

    if (!stagedVersion_.isEmpty() && QString::fromStdString(answer.info.version) != stagedVersion_)
    {
        DiscardStaged();
    }

    for (UpdateServiceObserver* observer : observers_)
    {
        observer->OnCheckFinished(true, answer.available, answer.info, {});
    }
}

void AppImageUpdateService::OnChecksumFinished()
{
    QNetworkReply* reply = Taken(checksumReply_);

    if (reply->error() != QNetworkReply::NoError)
    {
        SayTheStageFinished(false, HttpError(reply));
        return;
    }

    expectedChecksum_ = ParseSha256File(reply->readAll());
    if (expectedChecksum_.isEmpty())
    {
        SayTheStageFinished(false, tr("The checksum file is invalid."));
        return;
    }

    part_.setFileName(PartPath());
    if (!part_.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        SayTheStageFinished(false, tr("Could not save the downloaded file."));
        return;
    }

    hash_.reset();
    writeFailed_ = false;

    imageReply_ = StartGet(QString::fromStdString(pending_.appImageUrl));

    connect(imageReply_, &QNetworkReply::downloadProgress, this,
            [this](const qint64 received, const qint64 total)
            {
                for (UpdateServiceObserver* observer : observers_)
                {
                    observer->OnDownloadProgress(received, total);
                }
            });
    connect(imageReply_, &QNetworkReply::readyRead, this, &AppImageUpdateService::OnImageReadyRead);
    connect(imageReply_, &QNetworkReply::finished, this, &AppImageUpdateService::OnImageFinished);
}

void AppImageUpdateService::OnImageReadyRead()
{
    if (writeFailed_)
    {
        return;
    }

    const QByteArray chunk = imageReply_->readAll();

    if (part_.write(chunk) != chunk.size())
    {
        writeFailed_ = true;
        imageReply_->abort();
        return;
    }

    hash_.addData(chunk);
}

void AppImageUpdateService::OnImageFinished()
{
    QNetworkReply* reply = Taken(imageReply_);

    if (!writeFailed_ && reply->error() == QNetworkReply::NoError)
    {
        const QByteArray rest = reply->readAll();

        writeFailed_ = part_.write(rest) != rest.size();
        hash_.addData(rest);
    }

    part_.close();

    if (writeFailed_)
    {
        QFile::remove(PartPath());
        SayTheStageFinished(false, tr("Could not save the downloaded file."));
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        QFile::remove(PartPath());
        SayTheStageFinished(false, HttpError(reply));
        return;
    }

    if (QString::fromLatin1(hash_.result().toHex()) != expectedChecksum_)
    {
        QFile::remove(PartPath());
        SayTheStageFinished(false, tr("The downloaded file failed verification and was discarded."));
        return;
    }

    if (!QFile::setPermissions(PartPath(), kRunnable) || !Renamed(PartPath(), StagedPath()))
    {
        QFile::remove(PartPath());
        SayTheStageFinished(false, tr("Could not save the downloaded file."));
        return;
    }

    stagedVersion_ = QString::fromStdString(pending_.version);

    SayTheStageFinished(true, {});
}

QNetworkReply* AppImageUpdateService::StartGet(const QString& url)
{
    return network_.get(GithubRequest(url, currentVersion_));
}

QString AppImageUpdateService::StagedPath() const
{
    return QFileInfo(appImagePath_).absolutePath() + QLatin1Char('/') + QLatin1String(kStagedName);
}

QString AppImageUpdateService::PartPath() const
{
    return StagedPath() + QLatin1String(kPartSuffix);
}

void AppImageUpdateService::SayTheStageFinished(const bool ok, const QString& error)
{
    if (!ok)
    {
        expectedChecksum_.clear();
    }

    for (UpdateServiceObserver* observer : observers_)
    {
        observer->OnStageFinished(ok, error.toStdString());
    }
}
