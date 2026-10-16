#ifndef FS_ORGANIZER_INFRASTRUCTURE_UPDATE_APP_IMAGE_UPDATE_SERVICE_H
#define FS_ORGANIZER_INFRASTRUCTURE_UPDATE_APP_IMAGE_UPDATE_SERVICE_H

#include <vector>

#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QNetworkAccessManager>

#include "application/ports/UpdateService.h"
#include "infrastructure/update/GithubReleaseFeed.h"

class QNetworkReply;

class AppImageUpdateService final : public QObject, public UpdateService
{
    Q_OBJECT

public:
    AppImageUpdateService(QString feedUrl,
                          QString currentVersion,
                          QString appImagePath,
                          qint64 ownerPid,
                          QObject* parent = nullptr);

    void CheckForUpdates() override;

    void DownloadAndStage(const UpdateInfo& info) override;

    void DiscardStaged() override;

    [[nodiscard]] bool HasStagedUpdate() const override;

    bool LaunchApplyHelper(bool relaunch) override;

    void AddObserver(UpdateServiceObserver* observer) override;

    void RemoveObserver(UpdateServiceObserver* observer) override;

private:
    void OnCheckFinished(const FeedAnswer& answer);

    void OnChecksumFinished();

    void OnImageReadyRead();

    void OnImageFinished();

    [[nodiscard]] QNetworkReply* StartGet(const QString& url);

    [[nodiscard]] QString StagedPath() const;

    [[nodiscard]] QString PartPath() const;

    [[nodiscard]] bool Relaunch() const;

    void SayTheStageFinished(bool ok, const QString& error);

    QNetworkAccessManager network_;
    QString currentVersion_;
    QString appImagePath_;
    qint64 ownerPid_;
    std::vector<UpdateServiceObserver*> observers_;
    GithubReleaseFeed feed_;

    QNetworkReply* checksumReply_ = nullptr;
    QNetworkReply* imageReply_ = nullptr;
    QFile part_;
    QCryptographicHash hash_{QCryptographicHash::Sha256};
    bool writeFailed_ = false;

    UpdateInfo pending_;
    QString expectedChecksum_;
    QString stagedVersion_;
    bool applied_ = false;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_UPDATE_APP_IMAGE_UPDATE_SERVICE_H
