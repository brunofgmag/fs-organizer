#include "infrastructure/catalog/JsonManifestParser.h"

#include <string>
#include <vector>

#include <QtCore/QByteArray>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

namespace
{
    std::string TextField(const QJsonObject& object, const char* key)
    {
        return object.value(QLatin1StringView(key)).toString().toStdString();
    }

    std::vector<DeclaredDependency> DeclaredDependenciesIn(const QJsonObject& object)
    {
        std::vector<DeclaredDependency> declared;

        for (const QJsonValue entry : object.value(QLatin1StringView("dependencies")).toArray())
        {
            const QJsonObject fields = entry.toObject();
            std::string name = TextField(fields, "name");

            if (!name.empty())
            {
                declared.push_back({std::move(name), TextField(fields, "package_version")});
            }
        }

        return declared;
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
    manifest.packageOrderHint = TextField(object, "package_order_hint");
    manifest.packageVersion = TextField(object, "package_version");
    manifest.minimumGameVersion = TextField(object, "minimum_game_version");
    manifest.dependencies = DeclaredDependenciesIn(object);

    return manifest;
}
