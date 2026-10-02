#include "infrastructure/sim/RemovedStartupEntriesFile.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <QtCore/QByteArray>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include "domain/support/PathUtils.h"
#include "support/FileWriting.h"

namespace
{
    constexpr auto kEntries = "entries";
    constexpr auto kLabel = "label";
    constexpr auto kPath = "path";
    constexpr auto kCommandLine = "commandLine";
    constexpr auto kEnabled = "enabled";
    constexpr auto kBlock = "block";
    constexpr auto kFollowedBy = "followedBy";
    constexpr auto kRemovedAt = "removedAt";
    constexpr auto kFileSuffix = ".json";

    [[nodiscard]] QString TextOf(const std::string& utf8)
    {
        return QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size()));
    }

    [[nodiscard]] QString TextOf(const std::filesystem::path& path)
    {
        return TextOf(AsUtf8(path));
    }

    [[nodiscard]] QJsonObject ToJson(const RemovedStartupEntry& entry)
    {
        QJsonObject object;
        object[kLabel] = TextOf(entry.label);
        object[kPath] = TextOf(entry.path);
        object[kCommandLine] = TextOf(entry.commandLine);
        object[kEnabled] = entry.enabled;
        object[kBlock] =
            QString::fromLatin1(QByteArray(entry.block.data(), static_cast<qsizetype>(entry.block.size())).toBase64());
        object[kFollowedBy] = TextOf(entry.followedBy);
        object[kRemovedAt] = static_cast<qint64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(entry.removedAt.time_since_epoch()).count());

        return object;
    }

    [[nodiscard]] RemovedStartupEntry EntryFromJson(const QJsonObject& object)
    {
        const QByteArray block = QByteArray::fromBase64(object.value(kBlock).toString().toLatin1());

        return RemovedStartupEntry{.label = object.value(kLabel).toString().toStdString(),
                                   .path = PathFromUtf8(object.value(kPath).toString().toStdString()),
                                   .commandLine = object.value(kCommandLine).toString().toStdString(),
                                   .enabled = object.value(kEnabled).toBool(true),
                                   .block = std::string(block.constData(), static_cast<std::size_t>(block.size())),
                                   .followedBy = PathFromUtf8(object.value(kFollowedBy).toString().toStdString()),
                                   .removedAt = std::chrono::system_clock::time_point(
                                       std::chrono::duration_cast<std::chrono::system_clock::duration>(
                                           std::chrono::milliseconds(object.value(kRemovedAt).toInteger())))};
    }

    [[nodiscard]] std::string_view NameOfTheVariant(const SimulatorVariant variant)
    {
        return variant == SimulatorVariant::MSFS2020 ? "msfs2020" : "msfs2024";
    }

    [[nodiscard]] bool IsAt(const RemovedStartupEntry& entry, const std::filesystem::path& entryPath)
    {
        return ComparablePath(entry.path) == ComparablePath(entryPath);
    }
}

std::filesystem::path RemovedStartupEntriesFileOf(const std::filesystem::path& folder, const SimulatorVariant variant)
{
    return folder / PathFromUtf8(std::string(NameOfTheVariant(variant)) + kFileSuffix);
}

RemovedStartupEntriesFile::RemovedStartupEntriesFile(std::filesystem::path filePath) : filePath_(std::move(filePath))
{
}

std::optional<std::vector<RemovedStartupEntry>> RemovedStartupEntriesFile::Read() const
{
    if (filePath_.empty())
    {
        return std::vector<RemovedStartupEntry>{};
    }

    std::error_code error;
    const bool exists = std::filesystem::exists(filePath_, error);
    if (error)
    {
        return std::nullopt;
    }

    if (!exists)
    {
        return std::vector<RemovedStartupEntry>{};
    }

    std::ifstream stream(filePath_, std::ios::binary);
    if (!stream.is_open())
    {
        return std::nullopt;
    }

    const std::string content = std::string(std::istreambuf_iterator(stream), std::istreambuf_iterator<char>());

    QJsonParseError parsing;
    const QJsonDocument document = QJsonDocument::fromJson(
        QByteArray::fromRawData(content.data(), static_cast<qsizetype>(content.size())), &parsing);
    if (parsing.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::nullopt;
    }

    std::vector<RemovedStartupEntry> entries;

    for (const QJsonValue value : document.object().value(kEntries).toArray())
    {
        RemovedStartupEntry entry = EntryFromJson(value.toObject());

        if (!entry.path.empty())
        {
            entries.push_back(std::move(entry));
        }
    }

    return entries;
}

bool RemovedStartupEntriesFile::Write(const std::vector<RemovedStartupEntry>& entries) const
{
    QJsonArray array;

    for (const RemovedStartupEntry& entry : entries)
    {
        array.append(ToJson(entry));
    }

    QJsonObject root;
    root[kEntries] = array;

    std::error_code error;
    std::filesystem::create_directories(filePath_.parent_path(), error);

    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);

    return WriteFileReplacing(filePath_, std::string_view(json.constData(), static_cast<std::size_t>(json.size())));
}

std::vector<RemovedStartupEntry> RemovedStartupEntriesFile::Entries() const
{
    return Read().value_or(std::vector<RemovedStartupEntry>{});
}

bool RemovedStartupEntriesFile::Keep(const RemovedStartupEntry& entry) const
{
    if (filePath_.empty())
    {
        return false;
    }

    std::optional<std::vector<RemovedStartupEntry>> kept = Read();
    if (!kept.has_value())
    {
        return false;
    }

    kept->push_back(entry);

    return Write(*kept);
}

bool RemovedStartupEntriesFile::Drop(const std::filesystem::path& entryPath) const
{
    std::optional<std::vector<RemovedStartupEntry>> kept = Read();
    if (!kept.has_value())
    {
        return false;
    }

    const auto newest = std::find_if(kept->rbegin(), kept->rend(),
                                     [&entryPath](const RemovedStartupEntry& other)
                                     {
                                         return IsAt(other, entryPath);
                                     });

    if (newest == kept->rend())
    {
        return true;
    }

    kept->erase(std::next(newest).base());

    return Write(*kept);
}
