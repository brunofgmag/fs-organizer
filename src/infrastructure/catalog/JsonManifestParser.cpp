#include "infrastructure/catalog/JsonManifestParser.h"

#include <string>

#include <QtCore/QByteArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace
{
    std::string TextField(const QJsonObject& object, const char* key)
    {
        return object.value(QLatin1StringView(key)).toString().toStdString();
    }
}

std::optional<Manifest> JsonManifestParser::Parse(const std::string_view content) const
{
    const QJsonDocument document =
        QJsonDocument::fromJson(QByteArray::fromRawData(content.data(), static_cast<qsizetype>(content.size())));

    if (!document.isObject())
    {
        return std::nullopt;
    }

    const QJsonObject object = document.object();

    Manifest manifest;
    manifest.title = TextField(object, "title");
    manifest.creator = TextField(object, "creator");
    manifest.manufacturer = TextField(object, "manufacturer");
    manifest.contentType = TextField(object, "content_type");
    manifest.packageVersion = TextField(object, "package_version");
    manifest.minimumGameVersion = TextField(object, "minimum_game_version");

    return manifest;
}
