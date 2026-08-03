#include "infrastructure/update/GithubUpdateService.h"

#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>
#include <QtNetwork/QNetworkReply>

#include "infrastructure/update/GithubReleaseParser.h"

namespace
{
    constexpr auto kZipRootFolder = "fs-organizer";
    constexpr int kTransferTimeoutMs = 30000;

    constexpr auto kApplyScript =
        R"PS(param([int]$AppPid, [string]$Source, [string]$Dest, [string]$ExeName, [int]$Relaunch)
$root = Split-Path -Parent $PSCommandPath
Start-Transcript -Path (Join-Path $root 'apply.log') -Append | Out-Null
Wait-Process -Id $AppPid -Timeout 60 -ErrorAction SilentlyContinue
if (-not (Test-Path (Join-Path $Dest $ExeName))) { Stop-Transcript | Out-Null; exit 1 }
robocopy $Source $Dest /MIR /R:20 /W:1
if ($LASTEXITCODE -ge 8) { Stop-Transcript | Out-Null; exit 1 }
if ($Relaunch -eq 1) { Start-Process -FilePath (Join-Path $Dest $ExeName) -WorkingDirectory $Dest }
Stop-Transcript | Out-Null
Remove-Item -Recurse -Force (Join-Path $root 'download'), (Join-Path $root 'staged') -ErrorAction SilentlyContinue
Remove-Item -Force $PSCommandPath -ErrorAction SilentlyContinue
)PS";

    QString HttpError(const QNetworkReply* reply)
    {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        return status > 0 ? QStringLiteral("HTTP %1").arg(status) : reply->errorString();
    }

    QNetworkReply* Taken(QNetworkReply*& member)
    {
        QNetworkReply* reply = std::exchange(member, nullptr);
        reply->deleteLater();

        return reply;
    }

    bool WriteTheDownload(QNetworkReply* reply, const QString& zipPath)
    {
        QFile zip(zipPath);
        if (!zip.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return false;
        }

        zip.write(reply->readAll());
        zip.close();

        return true;
    }

    bool WriteTheApplyScript(const QString& scriptPath)
    {
        QFile script(scriptPath);
        if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return false;
        }

        script.write(kApplyScript);
        script.close();

        return true;
    }
}

GithubUpdateService::GithubUpdateService(QString feedUrl,
                                         QString currentVersion,
                                         QString updatesFolder,
                                         QObject* parent)
    : QObject(parent),
      feedUrl_(std::move(feedUrl)),
      currentVersion_(std::move(currentVersion)),
      updatesFolder_(std::move(updatesFolder))
{
    network_.setTransferTimeout(kTransferTimeoutMs);
}

QString GithubUpdateService::DefaultUpdatesFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/updates");
}

void GithubUpdateService::CheckForUpdates()
{
    if (checkReply_ != nullptr || feedUrl_.isEmpty())
    {
        return;
    }

    checkReply_ = StartGet(feedUrl_);

    connect(checkReply_, &QNetworkReply::finished, this, &GithubUpdateService::OnCheckFinished);
}

void GithubUpdateService::DownloadAndStage(const UpdateInfo& info)
{
    if (checksumReply_ != nullptr || zipReply_ != nullptr || extraction_ != nullptr)
    {
        return;
    }

    if (info.zipUrl.empty() || info.shaUrl.empty())
    {
        SayTheStageFinished(false, tr("This version did not bring the files to download."));
        return;
    }

    DiscardStaged();
    pending_ = info;

    checksumReply_ = StartGet(QString::fromStdString(info.shaUrl));

    connect(checksumReply_, &QNetworkReply::finished, this, &GithubUpdateService::OnChecksumFinished);
}

void GithubUpdateService::DiscardStaged()
{
    if (checksumReply_ != nullptr)
    {
        checksumReply_->abort();
    }

    if (zipReply_ != nullptr)
    {
        zipReply_->abort();
    }

    QDir(UpdatesFolder() + QStringLiteral("/download")).removeRecursively();
    QDir(UpdatesFolder() + QStringLiteral("/staged")).removeRecursively();

    stagedVersion_.clear();
}

bool GithubUpdateService::HasStagedUpdate() const
{
    return !stagedVersion_.isEmpty();
}

bool GithubUpdateService::LaunchApplyHelper(const bool relaunch)
{
    if (helperLaunched_)
    {
        return true;
    }

    const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    const QString stagedExe = StagedFolder() + QStringLiteral("/") + exeName;

    if (stagedVersion_.isEmpty() || !QFile::exists(stagedExe))
    {
        return false;
    }

    const QString scriptPath = UpdatesFolder() + QStringLiteral("/apply.ps1");
    if (!WriteTheApplyScript(scriptPath))
    {
        return false;
    }

    helperLaunched_ =
        QProcess::startDetached(QStringLiteral("powershell.exe"), ApplyArguments(scriptPath, exeName, relaunch));

    return helperLaunched_;
}

QStringList
GithubUpdateService::ApplyArguments(const QString& scriptPath, const QString& exeName, const bool relaunch) const
{
    return {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-WindowStyle"),
        QStringLiteral("Hidden"),
        QStringLiteral("-File"),
        QDir::toNativeSeparators(scriptPath),
        QStringLiteral("-AppPid"),
        QString::number(QCoreApplication::applicationPid()),
        QStringLiteral("-Source"),
        QDir::toNativeSeparators(StagedFolder()),
        QStringLiteral("-Dest"),
        QDir::toNativeSeparators(QCoreApplication::applicationDirPath()),
        QStringLiteral("-ExeName"),
        exeName,
        QStringLiteral("-Relaunch"),
        relaunch ? QStringLiteral("1") : QStringLiteral("0"),
    };
}

void GithubUpdateService::AddObserver(UpdateServiceObserver* observer)
{
    observers_.push_back(observer);
}

void GithubUpdateService::RemoveObserver(UpdateServiceObserver* observer)
{
    std::erase(observers_, observer);
}

void GithubUpdateService::OnCheckFinished()
{
    QNetworkReply* reply = Taken(checkReply_);

    if (reply->error() != QNetworkReply::NoError)
    {
        SayTheCheckFinished(false, false, {}, HttpError(reply));
        return;
    }

    const std::optional<UpdateInfo> info = ParseLatestRelease(reply->readAll());
    if (!info.has_value())
    {
        SayTheCheckFinished(false, false, {}, tr("GitHub answered in a format the app does not understand."));
        return;
    }

    if (!stagedVersion_.isEmpty() && QString::fromStdString(info->version) != stagedVersion_)
    {
        DiscardStaged();
    }

    SayTheCheckFinished(true, IsNewerVersion(QString::fromStdString(info->version), currentVersion_), *info, {});
}

void GithubUpdateService::OnChecksumFinished()
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
        SayTheStageFinished(false, tr("The checksum file came in invalid."));
        return;
    }

    QDir().mkpath(UpdatesFolder() + QStringLiteral("/download"));

    zipReply_ = StartGet(QString::fromStdString(pending_.zipUrl));

    connect(zipReply_, &QNetworkReply::downloadProgress, this,
            [this](const qint64 received, const qint64 total)
            {
                for (UpdateServiceObserver* observer : observers_)
                {
                    observer->OnDownloadProgress(received, total);
                }
            });
    connect(zipReply_, &QNetworkReply::finished, this, &GithubUpdateService::OnDownloadFinished);
}

void GithubUpdateService::OnDownloadFinished()
{
    QNetworkReply* reply = Taken(zipReply_);

    if (reply->error() != QNetworkReply::NoError)
    {
        SayTheStageFinished(false, HttpError(reply));
        return;
    }

    const QString zipPath = UpdatesFolder() + QStringLiteral("/download/") + QString::fromStdString(pending_.zipName);

    if (!WriteTheDownload(reply, zipPath))
    {
        SayTheStageFinished(false, tr("The downloaded file could not be written."));
        return;
    }

    switch (VerifyChecksum(zipPath))
    {
    case ChecksumVerdict::CouldNotBeRead:
        SayTheStageFinished(false, tr("The downloaded file could not be read, and was kept for you to inspect."));
        return;
    case ChecksumVerdict::DoesNotMatch:
        SayTheStageFinished(false, tr("The downloaded file does not match the checksum, and was discarded."));
        return;
    case ChecksumVerdict::Matches: break;
    }

    StartExtraction(zipPath, UpdatesFolder() + QStringLiteral("/staged"));
}

GithubUpdateService::ChecksumVerdict GithubUpdateService::VerifyChecksum(const QString& zipPath) const
{
    QFile zip(zipPath);
    if (!zip.open(QIODevice::ReadOnly))
    {
        return ChecksumVerdict::CouldNotBeRead;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&zip);
    zip.close();

    if (QString::fromLatin1(hash.result().toHex()) != expectedChecksum_)
    {
        QFile::remove(zipPath);

        return ChecksumVerdict::DoesNotMatch;
    }

    return ChecksumVerdict::Matches;
}

void GithubUpdateService::StartExtraction(const QString& zipPath, const QString& stagedRoot)
{
    static_cast<void>(QDir().mkpath(stagedRoot));

    extraction_ = new QProcess(this);

    connect(extraction_, &QProcess::finished, this,
            [this](const int exitCode, QProcess::ExitStatus)
            {
                OnExtractionFinished(exitCode);
            });

    extraction_->start(QStringLiteral("powershell.exe"),
                       {QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                        QStringLiteral("-NonInteractive"), QStringLiteral("-Command"),
                        QStringLiteral("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                            .arg(QDir::toNativeSeparators(zipPath), QDir::toNativeSeparators(stagedRoot))});
}

void GithubUpdateService::OnExtractionFinished(const int exitCode)
{
    extraction_->deleteLater();
    extraction_ = nullptr;

    const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();

    if (exitCode != 0 || !QFile::exists(StagedFolder() + QStringLiteral("/") + exeName))
    {
        SayTheStageFinished(false, tr("The update package could not be opened."));
        return;
    }

    stagedVersion_ = QString::fromStdString(pending_.version);

    SayTheStageFinished(true, {});
}

QNetworkReply* GithubUpdateService::StartGet(const QString& url)
{
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", QStringLiteral("fs-organizer/%1").arg(currentVersion_).toUtf8());

    return network_.get(request);
}

QString GithubUpdateService::UpdatesFolder() const
{
    return updatesFolder_;
}

QString GithubUpdateService::StagedFolder() const
{
    return UpdatesFolder() + QStringLiteral("/staged/") + QLatin1String(kZipRootFolder);
}

void GithubUpdateService::SayTheCheckFinished(const bool ok,
                                              const bool available,
                                              const UpdateInfo& info,
                                              const QString& error) const
{
    for (UpdateServiceObserver* observer : observers_)
    {
        observer->OnCheckFinished(ok, available, info, error.toStdString());
    }
}

void GithubUpdateService::SayTheStageFinished(const bool ok, const QString& error)
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
