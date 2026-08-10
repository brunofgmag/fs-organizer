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

#include "domain/support/PathSegment.h"
#include "domain/support/PathUtils.h"
#include "support/FileClock.h"
#include "support/FileWriting.h"

namespace
{
    constexpr auto kName = "name";
    constexpr auto kEntries = "entries";
    constexpr auto kLibraryId = "libraryId";
    constexpr auto kFolderName = "folderName";
    constexpr auto kAction = "action";
    constexpr auto kStartupEntries = "startupEntries";
    constexpr auto kGovernsStartup = "governsStartup";
    constexpr auto kPath = "path";
    constexpr auto kReturnFileSuffix = ".return.json";

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

    QJsonObject ToJson(const PresetStartupEntry& entry)
    {
        QJsonObject object;
        object[kPath] = QString::fromStdString(AsUtf8(entry.path));
        object[kAction] = ActionName(entry.action);

        return object;
    }

    PresetStartupEntry StartupEntryFromJson(const QJsonObject& object)
    {
        PresetStartupEntry entry;
        entry.path = PathFromUtf8(object.value(kPath).toString().toStdString());
        entry.action = ActionFromName(object.value(kAction));

        return entry;
    }

    std::optional<Preset> ReadPresetFrom(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
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
        preset.governsStartup = root.value(kGovernsStartup).toBool();

        for (const QJsonValue entry : root.value(kEntries).toArray())
        {
            preset.entries.push_back(EntryFromJson(entry.toObject()));
        }

        for (const QJsonValue entry : root.value(kStartupEntries).toArray())
        {
            preset.startupEntries.push_back(StartupEntryFromJson(entry.toObject()));
        }

        return preset;
    }

    bool WritePresetTo(const std::filesystem::path& file, const Preset& preset)
    {
        QJsonArray entries;
        for (const PresetEntry& entry : preset.entries)
        {
            entries.append(ToJson(entry));
        }

        QJsonArray startupEntries;
        for (const PresetStartupEntry& entry : preset.startupEntries)
        {
            startupEntries.append(ToJson(entry));
        }

        QJsonObject root;
        root[kName] = QString::fromStdString(preset.name);
        root[kEntries] = entries;
        root[kStartupEntries] = startupEntries;
        root[kGovernsStartup] = preset.governsStartup;

        std::error_code error;
        std::filesystem::create_directories(file.parent_path(), error);

        const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);

        return WriteFileReplacing(file, {json.constData(), static_cast<std::size_t>(json.size())});
    }
}

FilePresetRepository::FilePresetRepository(std::filesystem::path root) : root_(std::move(root))
{
}

std::vector<PresetListing> FilePresetRepository::List(const std::string& profileId) const
{
    const std::optional<std::filesystem::path> folder = FolderOf(profileId);
    if (!folder.has_value())
    {
        return {};
    }

    std::error_code error;
    std::filesystem::directory_iterator entry(*folder, error);
    if (error)
    {
        return {};
    }

    std::vector<PresetListing> listings;
    const std::filesystem::directory_iterator end;

    while (entry != end)
    {
        if (entry->is_regular_file(error) && entry->path().extension() == ".json")
        {
            std::error_code unread;
            const std::filesystem::file_time_type written = entry->last_write_time(unread);

            listings.push_back({.name = entry->path().stem().string(),
                                .writtenAt = unread ? std::nullopt : std::optional(SystemTimeOf(written))});
        }

        entry.increment(error);
        if (error)
        {
            return {};
        }
    }

    std::ranges::sort(listings, {}, &PresetListing::name);

    return listings;
}

std::optional<Preset> FilePresetRepository::Load(const std::string& profileId, const std::string& name) const
{
    const std::optional<std::filesystem::path> file = FileOf(profileId, name);

    return file.has_value() ? ReadPresetFrom(*file) : std::nullopt;
}

bool FilePresetRepository::Save(const std::string& profileId, const Preset& preset)
{
    const std::optional<std::filesystem::path> file = FileOf(profileId, preset.name);

    return file.has_value() && WritePresetTo(*file, preset);
}

std::optional<Preset> FilePresetRepository::LoadReturnPreset(const std::string& profileId) const
{
    const std::optional<std::filesystem::path> file = ReturnFileOf(profileId);

    return file.has_value() ? ReadPresetFrom(*file) : std::nullopt;
}

bool FilePresetRepository::SaveReturnPreset(const std::string& profileId, const Preset& preset)
{
    const std::optional<std::filesystem::path> file = ReturnFileOf(profileId);

    return file.has_value() && WritePresetTo(*file, preset);
}

bool FilePresetRepository::Rename(const std::string& profileId, const std::string& from, const std::string& to)
{
    std::optional<Preset> preset = Load(profileId, from);

    if (!preset.has_value())
    {
        return false;
    }

    const std::optional<std::filesystem::path> source = FileOf(profileId, from);
    const std::optional<std::filesystem::path> destination = FileOf(profileId, to);
    if (!source.has_value() || !destination.has_value())
    {
        return false;
    }

    const bool sameFile = ComparablePath(*source) == ComparablePath(*destination);

    preset->name = to;

    if (!Save(profileId, *preset))
    {
        return false;
    }

    if (!sameFile)
    {
        Remove(profileId, from);
    }

    return true;
}

void FilePresetRepository::Remove(const std::string& profileId, const std::string& name)
{
    const std::optional<std::filesystem::path> file = FileOf(profileId, name);
    if (!file.has_value())
    {
        return;
    }

    std::error_code error;
    std::filesystem::remove(*file, error);
}

std::optional<std::filesystem::path> FilePresetRepository::FolderOf(const std::string& profileId) const
{
    const std::optional<PathSegment> segment = PathSegment::From(profileId);

    return segment.has_value() ? std::optional(root_ / segment->Text()) : std::nullopt;
}

std::optional<std::filesystem::path> FilePresetRepository::FileOf(const std::string& profileId,
                                                                  const std::string& name) const
{
    const std::optional<std::filesystem::path> folder = FolderOf(profileId);
    const std::optional<PathSegment> segment = PathSegment::From(name);

    if (!folder.has_value() || !segment.has_value())
    {
        return std::nullopt;
    }

    return *folder / (segment->Text() + ".json");
}

std::optional<std::filesystem::path> FilePresetRepository::ReturnFileOf(const std::string& profileId) const
{
    const std::optional<PathSegment> segment = PathSegment::From(profileId);

    return segment.has_value() ? std::optional(root_ / (segment->Text() + kReturnFileSuffix)) : std::nullopt;
}
