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

#include "support/FileWriting.h"
#include "support/PathText.h"

namespace
{
    constexpr auto kActiveProfileId = "activeProfileId";
    constexpr auto kProfiles = "profiles";
    constexpr auto kLinkType = "linkType";
    constexpr auto kVerifyWithHash = "verifyWithHash";
    constexpr auto kManageStartupEntries = "manageStartupEntries";
    constexpr auto kManagePackageList = "managePackageList";
    constexpr auto kCoexistingAirports = "coexistingAirports";
    constexpr auto kOne = "one";
    constexpr auto kOther = "other";
    constexpr auto kFolderName = "folderName";
    constexpr auto kUpdateMode = "updateMode";
    constexpr auto kLanguage = "language";
    constexpr auto kId = "id";
    constexpr auto kVariant = "variant";
    constexpr auto kDestinations = "destinations";
    constexpr auto kDefaultDestination = "defaultDestination";
    constexpr auto kLibraries = "libraries";
    constexpr auto kPath = "path";
    constexpr auto kLabel = "label";
    constexpr auto kDestinationOverrides = "destinationOverrides";
    constexpr auto kExternalOrigins = "externalOrigins";
    constexpr auto kLibraryId = "libraryId";
    constexpr auto kRelativePath = "relativePath";
    constexpr auto kDestination = "destination";
    constexpr auto kExternalPath = "externalPath";

    std::filesystem::path ToPath(const QJsonValue& value)
    {
        return AsPath(value.toString());
    }

    QString VariantName(const SimulatorVariant variant)
    {
        switch (variant)
        {
        case SimulatorVariant::MSFS2020: return "MSFS2020";
        case SimulatorVariant::MSFS2024: break;
        }

        return "MSFS2024";
    }

    SimulatorVariant VariantFromName(const QJsonValue& value)
    {
        return value.toString() == "MSFS2020" ? SimulatorVariant::MSFS2020 : SimulatorVariant::MSFS2024;
    }

    QString LinkTypeName(const LinkType linkType)
    {
        switch (linkType)
        {
        case LinkType::Symbolic: return "symbolic";
        case LinkType::Junction: break;
        }

        return "junction";
    }

    LinkType LinkTypeFromName(const QJsonValue& value)
    {
        return value.toString() == "symbolic" ? LinkType::Symbolic : LinkType::Junction;
    }

    QString UpdateModeName(const UpdateMode updateMode)
    {
        switch (updateMode)
        {
        case UpdateMode::Automatic: return "automatic";
        case UpdateMode::Manual: return "manual";
        case UpdateMode::Notify: break;
        }

        return "notify";
    }

    UpdateMode UpdateModeFromName(const QJsonValue& value)
    {
        if (value.toString() == "automatic")
        {
            return UpdateMode::Automatic;
        }

        return value.toString() == "manual" ? UpdateMode::Manual : UpdateMode::Notify;
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

    QJsonObject ToJson(const ExternalOrigin& externalOrigin)
    {
        QJsonObject object;
        object[kLibraryId] = QString::fromStdString(externalOrigin.libraryId);
        object[kRelativePath] = AsText(externalOrigin.relativePath);
        object[kExternalPath] = AsText(externalOrigin.externalPath);

        return object;
    }

    ExternalOrigin ExternalOriginFromJson(const QJsonObject& object)
    {
        ExternalOrigin externalOrigin;
        externalOrigin.libraryId = object.value(kLibraryId).toString().toStdString();
        externalOrigin.relativePath = ToPath(object.value(kRelativePath));
        externalOrigin.externalPath = ToPath(object.value(kExternalPath));

        return externalOrigin;
    }

    QJsonObject ToJson(const AddonId& addon)
    {
        QJsonObject object;
        object[kLibraryId] = QString::fromStdString(addon.libraryId);
        object[kFolderName] = QString::fromStdString(addon.folderName);

        return object;
    }

    AddonId AddonFromJson(const QJsonObject& object)
    {
        return {.libraryId = object.value(kLibraryId).toString().toStdString(),
                .folderName = object.value(kFolderName).toString().toStdString()};
    }

    QJsonObject ToJson(const CoexistingPair& pair)
    {
        QJsonObject object;
        object[kOne] = ToJson(pair.one);
        object[kOther] = ToJson(pair.other);

        return object;
    }

    CoexistingPair CoexistingPairFromJson(const QJsonObject& object)
    {
        return {.one = AddonFromJson(object.value(kOne).toObject()),
                .other = AddonFromJson(object.value(kOther).toObject())};
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

        QJsonArray externalOrigins;
        for (const ExternalOrigin& externalOrigin : profile.externalOrigins)
        {
            externalOrigins.append(ToJson(externalOrigin));
        }

        QJsonObject object;
        object[kId] = QString::fromStdString(profile.id);
        object[kVariant] = VariantName(profile.variant);
        object[kDestinations] = destinations;
        object[kDefaultDestination] = AsText(profile.defaultDestination);
        object[kLibraries] = libraries;
        object[kDestinationOverrides] = overrides;
        object[kExternalOrigins] = externalOrigins;

        return object;
    }

    SimulatorProfile ProfileFromJson(const QJsonObject& object)
    {
        SimulatorProfile profile;
        profile.id = object.value(kId).toString().toStdString();
        profile.variant = VariantFromName(object.value(kVariant));
        profile.defaultDestination = ToPath(object.value(kDefaultDestination));

        for (const QJsonValue destination : object.value(kDestinations).toArray())
        {
            profile.destinations.push_back(ToPath(destination));
        }

        for (const QJsonValue library : object.value(kLibraries).toArray())
        {
            profile.libraries.push_back(LibraryFromJson(library.toObject()));
        }

        for (const QJsonValue destinationOverride : object.value(kDestinationOverrides).toArray())
        {
            profile.destinationOverrides.push_back(OverrideFromJson(destinationOverride.toObject()));
        }

        for (const QJsonValue externalOrigin : object.value(kExternalOrigins).toArray())
        {
            profile.externalOrigins.push_back(ExternalOriginFromJson(externalOrigin.toObject()));
        }

        return profile;
    }
}

JsonSettingsRepository::JsonSettingsRepository(std::filesystem::path file) : file_(std::move(file))
{
}

std::optional<AppSettings> JsonSettingsRepository::Load() const
{
    std::error_code error;
    if (!std::filesystem::exists(file_, error) || error)
    {
        return AppSettings{};
    }

    std::ifstream stream(file_, std::ios::binary);
    const std::string content{std::istreambuf_iterator(stream), std::istreambuf_iterator<char>()};

    const QJsonDocument document =
        QJsonDocument::fromJson(QByteArray::fromRawData(content.data(), static_cast<qsizetype>(content.size())));
    if (!document.isObject())
    {
        return std::nullopt;
    }

    const QJsonObject root = document.object();

    AppSettings settings;
    settings.activeProfileId = root.value(kActiveProfileId).toString().toStdString();
    settings.linkType = LinkTypeFromName(root.value(kLinkType));
    settings.verifyWithHash = root.value(kVerifyWithHash).toBool(false);
    settings.manageStartupEntries = root.value(kManageStartupEntries).toBool(true);
    settings.managePackageList = root.value(kManagePackageList).toBool(false);
    settings.updateMode = UpdateModeFromName(root.value(kUpdateMode));
    settings.language = root.value(kLanguage).toString().toStdString();

    for (const QJsonValue profile : root.value(kProfiles).toArray())
    {
        settings.profiles.push_back(ProfileFromJson(profile.toObject()));
    }

    for (const QJsonValue pair : root.value(kCoexistingAirports).toArray())
    {
        settings.coexistingAirports.push_back(CoexistingPairFromJson(pair.toObject()));
    }

    return settings;
}

bool JsonSettingsRepository::Save(const AppSettings& settings)
{
    QJsonArray profiles;
    for (const SimulatorProfile& profile : settings.profiles)
    {
        profiles.append(ToJson(profile));
    }

    QJsonArray coexisting;
    for (const CoexistingPair& pair : settings.coexistingAirports)
    {
        coexisting.append(ToJson(pair));
    }

    QJsonObject root;
    root[kActiveProfileId] = QString::fromStdString(settings.activeProfileId);
    root[kProfiles] = profiles;
    root[kLinkType] = LinkTypeName(settings.linkType);
    root[kVerifyWithHash] = settings.verifyWithHash;
    root[kManageStartupEntries] = settings.manageStartupEntries;
    root[kManagePackageList] = settings.managePackageList;
    root[kUpdateMode] = UpdateModeName(settings.updateMode);
    root[kLanguage] = QString::fromStdString(settings.language);
    root[kCoexistingAirports] = coexisting;

    std::error_code error;
    std::filesystem::create_directories(file_.parent_path(), error);

    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);

    return WriteFileReplacing(file_, {json.constData(), static_cast<std::size_t>(json.size())});
}
