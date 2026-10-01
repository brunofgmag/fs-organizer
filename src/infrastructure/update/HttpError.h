#ifndef FS_ORGANIZER_INFRASTRUCTURE_UPDATE_HTTP_ERROR_H
#define FS_ORGANIZER_INFRASTRUCTURE_UPDATE_HTTP_ERROR_H

#include <QtCore/QString>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

inline constexpr int kFirstStatusThatIsAnError = 400;

[[nodiscard]] inline QString HttpError(const QNetworkReply* reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    return status >= kFirstStatusThatIsAnError ? QStringLiteral("HTTP %1").arg(status) : reply->errorString();
}

#endif // FS_ORGANIZER_INFRASTRUCTURE_UPDATE_HTTP_ERROR_H
