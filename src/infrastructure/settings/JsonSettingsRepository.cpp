#include "infrastructure/settings/JsonSettingsRepository.h"

#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

#include <QtCore/QByteArray>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include "support/PathText.h"

namespace
{
    constexpr auto kActiveProfileId = "activeProfileId";
    constexpr auto kProfiles = "profiles";
    constexpr auto kId = "id";
    constexpr auto kVariant = "variant";
    constexpr auto kDestinations = "destinations";
    constexpr auto kDefaultDestination = "defaultDestination";
    constexpr auto kLibraries = "libraries";
    constexpr auto kPath = "path";
    constexpr auto kLabel = "label";
    constexpr auto kDestinationOverrides = "destinationOverrides";
    constexpr auto kLibraryId = "libraryId";
    constexpr auto kRelativePath = "relativePath";
    constexpr auto kDestination = "destination";

    std::filesystem::path ToPath(const QJsonValue& value)
    {
        return AsPath(value.toString());
    }

    QString VariantName(const SimulatorVariant variant)
    {
        return variant == SimulatorVariant::MSFS2020 ? "MSFS2020" : "MSFS2024";
    }

    SimulatorVariant VariantFromName(const QJsonValue& value)
    {
        return value.toString() == "MSFS2020" ? SimulatorVariant::MSFS2020 : SimulatorVariant::MSFS2024;
    }

    QJsonObject ToJson(const Library& library)
    {
        QJsonObject object;
        object[kId] = QString::fromStdString(library.id);
        object[kPath] = AsText(library.path);
        object[kLabel] = QString::fromStdString(library.label);

        return object;
    }

    Library LibraryFromJson(const QJsonObject& object)
    {
        Library library;
        library.id = object.value(kId).toString().toStdString();
        library.path = ToPath(object.value(kPath));
        library.label = object.value(kLabel).toString().toStdString();

        return library;
    }

    QJsonObject ToJson(const DestinationOverride& destinationOverride)
    {
        QJsonObject object;
        object[kLibraryId] = QString::fromStdString(destinationOverride.libraryId);
        object[kRelativePath] = AsText(destinationOverride.relativePath);
        object[kDestination] = AsText(destinationOverride.destination);

        return object;
    }

    DestinationOverride OverrideFromJson(const QJsonObject& object)
    {
        DestinationOverride destinationOverride;
        destinationOverride.libraryId = object.value(kLibraryId).toString().toStdString();
        destinationOverride.relativePath = ToPath(object.value(kRelativePath));
        destinationOverride.destination = ToPath(object.value(kDestination));

        return destinationOverride;
    }

    QJsonObject ToJson(const SimulatorProfile& profile)
    {
        QJsonArray destinations;
        for (const std::filesystem::path& destination : profile.destinations)
        {
            destinations.append(AsText(destination));
        }

        QJsonArray libraries;
        for (const Library& library : profile.libraries)
        {
            libraries.append(ToJson(library));
        }

        QJsonArray overrides;
        for (const DestinationOverride& destinationOverride : profile.destinationOverrides)
        {
            overrides.append(ToJson(destinationOverride));
        }

        QJsonObject object;
        object[kId] = QString::fromStdString(profile.id);
        object[kVariant] = VariantName(profile.variant);
        object[kDestinations] = destinations;
        object[kDefaultDestination] = AsText(profile.defaultDestination);
        object[kLibraries] = libraries;
        object[kDestinationOverrides] = overrides;

        return object;
    }

    SimulatorProfile ProfileFromJson(const QJsonObject& object)
    {
        SimulatorProfile profile;
        profile.id = object.value(kId).toString().toStdString();
        profile.variant = VariantFromName(object.value(kVariant));
        profile.defaultDestination = ToPath(object.value(kDefaultDestination));

        for (const QJsonValue& destination : object.value(kDestinations).toArray())
        {
            profile.destinations.push_back(ToPath(destination));
        }

        for (const QJsonValue& library : object.value(kLibraries).toArray())
        {
            profile.libraries.push_back(LibraryFromJson(library.toObject()));
        }

        for (const QJsonValue& destinationOverride : object.value(kDestinationOverrides).toArray())
        {
            profile.destinationOverrides.push_back(OverrideFromJson(destinationOverride.toObject()));
        }

        return profile;
    }
}

JsonSettingsRepository::JsonSettingsRepository(std::filesystem::path file) : file_(std::move(file))
{
}

AppSettings JsonSettingsRepository::Load() const
{
    std::ifstream stream(file_, std::ios::binary);
    const std::string content{std::istreambuf_iterator(stream), std::istreambuf_iterator<char>()};

    const QJsonDocument document =
        QJsonDocument::fromJson(QByteArray::fromRawData(content.data(), static_cast<qsizetype>(content.size())));
    if (!document.isObject())
    {
        return {};
    }

    const QJsonObject root = document.object();

    AppSettings settings;
    settings.activeProfileId = root.value(kActiveProfileId).toString().toStdString();

    for (const QJsonValue& profile : root.value(kProfiles).toArray())
    {
        settings.profiles.push_back(ProfileFromJson(profile.toObject()));
    }

    return settings;
}

void JsonSettingsRepository::Save(const AppSettings& settings)
{
    QJsonArray profiles;
    for (const SimulatorProfile& profile : settings.profiles)
    {
        profiles.append(ToJson(profile));
    }

    QJsonObject root;
    root[kActiveProfileId] = QString::fromStdString(settings.activeProfileId);
    root[kProfiles] = profiles;

    std::error_code error;
    std::filesystem::create_directories(file_.parent_path(), error);

    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    std::ofstream stream(file_, std::ios::binary | std::ios::trunc);
    stream.write(json.constData(), json.size());
}
