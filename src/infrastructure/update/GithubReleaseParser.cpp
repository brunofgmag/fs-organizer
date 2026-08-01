#include "infrastructure/update/GithubReleaseParser.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QVersionNumber>

namespace
{
    constexpr auto kAssetPrefix = "fs-organizer-";
    constexpr auto kZipSuffix = ".zip";
    constexpr auto kChecksumSuffix = ".zip.sha256";

    QString WithoutTheTagPrefix(const QString& tag)
    {
        QString trimmed = tag.trimmed();

        if (trimmed.startsWith(u'v') || trimmed.startsWith(u'V'))
        {
            return trimmed.mid(1);
        }

        return trimmed;
    }
}

std::optional<UpdateInfo> ParseLatestRelease(const QByteArray& json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isObject())
    {
        return std::nullopt;
    }

    const QJsonObject release = document.object();
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    if (tag.trimmed().isEmpty())
    {
        return std::nullopt;
    }

    UpdateInfo info;
    info.version = WithoutTheTagPrefix(tag).toStdString();
    info.releasePageUrl = release.value(QStringLiteral("html_url")).toString().toStdString();

    for (const QJsonValue value : release.value(QStringLiteral("assets")).toArray())
    {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        const QString url = asset.value(QStringLiteral("browser_download_url")).toString();

        if (!name.startsWith(QLatin1String(kAssetPrefix)))
        {
            continue;
        }

        if (name.endsWith(QLatin1String(kChecksumSuffix)))
        {
            info.shaUrl = url.toStdString();
        }
        else if (name.endsWith(QLatin1String(kZipSuffix)))
        {
            info.zipName = name.toStdString();
            info.zipUrl = url.toStdString();
        }
    }

    return info;
}

QString ParseSha256File(const QByteArray& content)
{
    static const QRegularExpression kSpace(QStringLiteral("\\s"));
    static const QRegularExpression kHexHash(QStringLiteral("^[0-9a-f]{64}$"));

    const QString hash = QString::fromUtf8(content).trimmed().section(kSpace, 0, 0).toLower();

    return kHexHash.match(hash).hasMatch() ? hash : QString{};
}

bool IsNewerVersion(const QString& tagName, const QString& currentVersion)
{
    const QVersionNumber latest = QVersionNumber::fromString(WithoutTheTagPrefix(tagName));
    const QVersionNumber current = QVersionNumber::fromString(WithoutTheTagPrefix(currentVersion));

    if (latest.isNull() || current.isNull())
    {
        return false;
    }

    return latest > current;
}
