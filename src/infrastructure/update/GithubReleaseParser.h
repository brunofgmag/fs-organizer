#ifndef FS_ORGANIZER_INFRASTRUCTURE_UPDATE_GITHUB_RELEASE_PARSER_H
#define FS_ORGANIZER_INFRASTRUCTURE_UPDATE_GITHUB_RELEASE_PARSER_H

#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include "application/model/UpdateInfo.h"

[[nodiscard]] std::optional<UpdateInfo> ParseLatestRelease(const QByteArray& json);

[[nodiscard]] QString ParseSha256File(const QByteArray& content);

[[nodiscard]] bool IsNewerVersion(const QString& tagName, const QString& currentVersion);

#endif // FS_ORGANIZER_INFRASTRUCTURE_UPDATE_GITHUB_RELEASE_PARSER_H
