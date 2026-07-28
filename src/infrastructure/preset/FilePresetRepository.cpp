#include "infrastructure/preset/FilePresetRepository.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include <QtCore/QByteArray>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

namespace
{
    constexpr auto kName = "name";
    constexpr auto kEntries = "entries";
    constexpr auto kLibraryId = "libraryId";
    constexpr auto kFolderName = "folderName";
    constexpr auto kAction = "action";

    QString ActionName(const PresetAction action)
    {
        return action == PresetAction::Disable ? "disable" : "enable";
    }

    PresetAction ActionFromName(const QJsonValue& value)
    {
        return value.toString() == "disable" ? PresetAction::Disable : PresetAction::Enable;
    }

    QJsonObject ToJson(const PresetEntry& entry)
    {
        QJsonObject object;
        object[kLibraryId] = QString::fromStdString(entry.addonId.libraryId);
        object[kFolderName] = QString::fromStdString(entry.addonId.folderName);
        object[kAction] = ActionName(entry.action);

        return object;
    }

    PresetEntry EntryFromJson(const QJsonObject& object)
    {
        PresetEntry entry;
        entry.addonId.libraryId = object.value(kLibraryId).toString().toStdString();
        entry.addonId.folderName = object.value(kFolderName).toString().toStdString();
        entry.action = ActionFromName(object.value(kAction));

        return entry;
    }
}

FilePresetRepository::FilePresetRepository(std::filesystem::path root) : root_(std::move(root))
{
}

std::vector<std::string> FilePresetRepository::List(const std::string& profileId) const
{
    std::error_code error;
    std::vector<std::string> names;

    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(FolderOf(profileId), error))
    {
        if (entry.is_regular_file(error) && entry.path().extension() == ".json")
        {
            names.push_back(entry.path().stem().string());
        }
    }

    std::ranges::sort(names);

    return names;
}

std::optional<Preset> FilePresetRepository::Load(const std::string& profileId, const std::string& name) const
{
    std::ifstream stream(FileOf(profileId, name), std::ios::binary);
    const std::string content{std::istreambuf_iterator(stream), std::istreambuf_iterator<char>()};

    const QJsonDocument document =
        QJsonDocument::fromJson(QByteArray::fromRawData(content.data(), static_cast<qsizetype>(content.size())));

    if (!document.isObject())
    {
        return std::nullopt;
    }

    const QJsonObject root = document.object();

    Preset preset;
    preset.name = root.value(kName).toString().toStdString();

    for (const QJsonValue& entry : root.value(kEntries).toArray())
    {
        preset.entries.push_back(EntryFromJson(entry.toObject()));
    }

    return preset;
}

bool FilePresetRepository::Save(const std::string& profileId, const Preset& preset)
{
    QJsonArray entries;
    for (const PresetEntry& entry : preset.entries)
    {
        entries.append(ToJson(entry));
    }

    QJsonObject root;
    root[kName] = QString::fromStdString(preset.name);
    root[kEntries] = entries;

    std::error_code error;
    std::filesystem::create_directories(FolderOf(profileId), error);

    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    std::ofstream stream(FileOf(profileId, preset.name), std::ios::binary | std::ios::trunc);
    stream.write(json.constData(), json.size());
    stream.flush();

    return stream.good();
}

bool FilePresetRepository::Rename(const std::string& profileId, const std::string& from, const std::string& to)
{
    std::optional<Preset> preset = Load(profileId, from);

    if (!preset.has_value())
    {
        return false;
    }

    preset->name = to;

    if (!Save(profileId, *preset))
    {
        return false;
    }

    Remove(profileId, from);

    return true;
}

void FilePresetRepository::Remove(const std::string& profileId, const std::string& name)
{
    std::error_code error;
    std::filesystem::remove(FileOf(profileId, name), error);
}

std::filesystem::path FilePresetRepository::FolderOf(const std::string& profileId) const
{
    return root_ / profileId;
}

std::filesystem::path FilePresetRepository::FileOf(const std::string& profileId, const std::string& name) const
{
    return FolderOf(profileId) / (name + ".json");
}
