#ifndef FS_ORGANIZER_INFRASTRUCTURE_UPDATE_GITHUB_RELEASE_FEED_H
#define FS_ORGANIZER_INFRASTRUCTURE_UPDATE_GITHUB_RELEASE_FEED_H

#include <functional>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>

#include "application/model/UpdateInfo.h"

class QNetworkReply;

struct FeedAnswer
{
    bool ok{};
    bool available{};
    UpdateInfo info{};
    QString error{};
};

[[nodiscard]] QNetworkRequest GithubRequest(const QString& url, const QString& currentVersion);

[[nodiscard]] QString HttpError(const QNetworkReply* reply);

class GithubReleaseFeed final : public QObject
{
    Q_OBJECT

public:
    GithubReleaseFeed(QString feedUrl,
                      QString currentVersion,
                      std::function<void(const FeedAnswer&)> answered,
                      QObject* parent = nullptr);

    void Ask();

private:
    void OnAnswered();

    QNetworkAccessManager network_;
    QString feedUrl_;
    QString currentVersion_;
    std::function<void(const FeedAnswer&)> answered_;

    QNetworkReply* asking_ = nullptr;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_UPDATE_GITHUB_RELEASE_FEED_H
