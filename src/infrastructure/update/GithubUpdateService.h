#ifndef FS_ORGANIZER_INFRASTRUCTURE_UPDATE_GITHUB_UPDATE_SERVICE_H
#define FS_ORGANIZER_INFRASTRUCTURE_UPDATE_GITHUB_UPDATE_SERVICE_H

#include <string>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtNetwork/QNetworkAccessManager>

#include "application/ports/UpdateService.h"

class QNetworkReply;
class QProcess;

class GithubUpdateService final : public QObject, public UpdateService
{
    Q_OBJECT

public:
    GithubUpdateService(QString feedUrl, QString currentVersion, QString updatesFolder, QObject* parent = nullptr);

    [[nodiscard]] static QString DefaultUpdatesFolder();

    void CheckForUpdates() override;

    void DownloadAndStage(const UpdateInfo& info) override;

    void DiscardStaged() override;

    [[nodiscard]] bool HasStagedUpdate() const override;

    bool LaunchApplyHelper(bool relaunch) override;

    void AddObserver(UpdateServiceObserver* observer) override;

    void RemoveObserver(UpdateServiceObserver* observer) override;

private:
    void OnCheckFinished();

    void OnChecksumFinished();

    void OnDownloadFinished();

    void OnExtractionFinished(int exitCode);

    [[nodiscard]] QNetworkReply* StartGet(const QString& url);

    [[nodiscard]] QString UpdatesFolder() const;

    [[nodiscard]] QString StagedFolder() const;

    enum class ChecksumVerdict
    {
        Matches,
        DoesNotMatch,
        CouldNotBeRead
    };

    [[nodiscard]] ChecksumVerdict VerifyChecksum(const QString& zipPath) const;

    void StartExtraction(const QString& zipPath, const QString& stagedRoot);

    [[nodiscard]] QStringList ApplyArguments(const QString& scriptPath, const QString& exeName, bool relaunch) const;

    void SayTheCheckFinished(bool ok, bool available, const UpdateInfo& info, const QString& error) const;

    void SayTheStageFinished(bool ok, const QString& error);

    QNetworkAccessManager network_;
    QString feedUrl_;
    QString currentVersion_;
    QString updatesFolder_;
    std::vector<UpdateServiceObserver*> observers_;

    QNetworkReply* checkReply_ = nullptr;
    QNetworkReply* checksumReply_ = nullptr;
    QNetworkReply* zipReply_ = nullptr;
    QProcess* extraction_ = nullptr;

    UpdateInfo pending_;
    QString expectedChecksum_;
    QString stagedVersion_;
    bool helperLaunched_ = false;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_UPDATE_GITHUB_UPDATE_SERVICE_H
