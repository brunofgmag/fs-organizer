#include "infrastructure/update/GithubReleaseFeed.h"

#include <optional>
#include <utility>

#include <QtCore/QUrl>
#include <QtNetwork/QNetworkReply>

#include "infrastructure/update/GithubReleaseParser.h"

namespace
{
    constexpr int kTransferTimeoutMs = 30000;
}

QNetworkRequest GithubRequest(const QString& url, const QString& currentVersion)
{
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", QStringLiteral("fs-organizer/%1").arg(currentVersion).toUtf8());

    return request;
}

QString HttpError(const QNetworkReply* reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    return status > 0 ? QStringLiteral("HTTP %1").arg(status) : reply->errorString();
}

GithubReleaseFeed::GithubReleaseFeed(QString feedUrl,
                                     QString currentVersion,
                                     std::function<void(const FeedAnswer&)> answered,
                                     QObject* parent)
    : QObject(parent),
      feedUrl_(std::move(feedUrl)),
      currentVersion_(std::move(currentVersion)),
      answered_(std::move(answered))
{
    network_.setTransferTimeout(kTransferTimeoutMs);
}

void GithubReleaseFeed::Ask()
{
    if (asking_ != nullptr || feedUrl_.isEmpty())
    {
        return;
    }

    asking_ = network_.get(GithubRequest(feedUrl_, currentVersion_));

    connect(asking_, &QNetworkReply::finished, this, &GithubReleaseFeed::OnAnswered);
}

void GithubReleaseFeed::OnAnswered()
{
    QNetworkReply* reply = std::exchange(asking_, nullptr);
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError)
    {
        answered_(FeedAnswer{.ok = false, .available = false, .info = {}, .error = HttpError(reply)});
        return;
    }

    const std::optional<UpdateInfo> info = ParseLatestRelease(reply->readAll());
    if (!info.has_value())
    {
        answered_(FeedAnswer{.ok = false,
                             .available = false,
                             .info = {},
                             .error = tr("GitHub sent a response the app could not read.")});
        return;
    }

    answered_(FeedAnswer{.ok = true,
                         .available = IsNewerVersion(QString::fromStdString(info->version), currentVersion_),
                         .info = *info,
                         .error = {}});
}
