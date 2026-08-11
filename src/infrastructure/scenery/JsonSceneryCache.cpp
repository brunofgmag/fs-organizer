#include "infrastructure/scenery/JsonSceneryCache.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>
#include <vector>

#include <QtCore/QByteArray>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include "domain/support/PathUtils.h"
#include "support/FileWriting.h"

namespace
{
    constexpr auto kAddons = "addons";
    constexpr auto kFolder = "folder";
    constexpr auto kReadAt = "readAt";
    constexpr auto kFiles = "files";
    constexpr auto kReading = "reading";
    constexpr auto kCodes = "codes";
    constexpr auto kDidNotDecode = "anIdentifierDidNotDecode";

    [[nodiscard]] qint64 MillisecondsOf(const std::chrono::system_clock::time_point moment)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(moment.time_since_epoch()).count();
    }

    [[nodiscard]] std::chrono::system_clock::time_point MomentOf(const qint64 milliseconds)
    {
        return std::chrono::system_clock::time_point{std::chrono::milliseconds{milliseconds}};
    }

    [[nodiscard]] SceneryReading ReadingNamed(const QString& name)
    {
        if (name == QStringLiteral("itCarriesNoSignature"))
        {
            return SceneryReading::ItCarriesNoSignature;
        }

        if (name == QStringLiteral("itEndsBeforeItSaysItDoes"))
        {
            return SceneryReading::ItEndsBeforeItSaysItDoes;
        }

        return SceneryReading::Read;
    }

    [[nodiscard]] QString NameOfReading(const SceneryReading reading)
    {
        switch (reading)
        {
        case SceneryReading::Read: return QStringLiteral("read");
        case SceneryReading::ItCarriesNoSignature: return QStringLiteral("itCarriesNoSignature");
        case SceneryReading::ItEndsBeforeItSaysItDoes: return QStringLiteral("itEndsBeforeItSaysItDoes");
        }

        return QStringLiteral("read");
    }

    [[nodiscard]] QJsonObject ToJson(const SceneryCodes& file)
    {
        QJsonArray codes;
        for (const std::string& code : file.codes)
        {
            codes.append(QString::fromStdString(code));
        }

        QJsonObject object;
        object[kReading] = NameOfReading(file.reading);
        object[kCodes] = codes;
        object[kDidNotDecode] = file.anIdentifierDidNotDecode;

        return object;
    }

    [[nodiscard]] SceneryCodes FileFromJson(const QJsonObject& object)
    {
        SceneryCodes file{.reading = ReadingNamed(object.value(kReading).toString()),
                          .codes = {},
                          .anIdentifierDidNotDecode = object.value(kDidNotDecode).toBool()};

        for (const QJsonValue& code : object.value(kCodes).toArray())
        {
            file.codes.push_back(code.toString().toStdString());
        }

        return file;
    }
}

JsonSceneryCache::JsonSceneryCache(std::filesystem::path filePath) : filePath_(std::move(filePath))
{
    Read();
}

void JsonSceneryCache::Read()
{
    std::ifstream stream(filePath_, std::ios::binary);
    if (!stream.is_open())
    {
        return;
    }

    const std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    const QJsonDocument document =
        QJsonDocument::fromJson(QByteArray::fromRawData(bytes.data(), static_cast<qsizetype>(bytes.size())));

    for (const QJsonValue& value : document.object().value(kAddons).toArray())
    {
        const QJsonObject addon = value.toObject();

        RememberedScenery scenery{.readAt = MomentOf(static_cast<qint64>(addon.value(kReadAt).toDouble())),
                                  .files = {}};

        for (const QJsonValue& file : addon.value(kFiles).toArray())
        {
            scenery.files.push_back(FileFromJson(file.toObject()));
        }

        known_.insert_or_assign(addon.value(kFolder).toString().toStdString(), std::move(scenery));
    }
}

void JsonSceneryCache::Write() const
{
    QJsonArray addons;

    for (const auto& [folder, scenery] : known_)
    {
        QJsonArray files;
        for (const SceneryCodes& file : scenery.files)
        {
            files.append(ToJson(file));
        }

        QJsonObject addon;
        addon[kFolder] = QString::fromStdString(folder);
        addon[kReadAt] = static_cast<double>(MillisecondsOf(scenery.readAt));
        addon[kFiles] = files;

        addons.append(addon);
    }

    QJsonObject root;
    root[kAddons] = addons;

    std::error_code failed;
    std::filesystem::create_directories(ParentOf(filePath_), failed);

    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);

    static_cast<void>(
        WriteFileReplacing(filePath_, std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size()))));
}

std::optional<RememberedScenery> JsonSceneryCache::Remember(const std::filesystem::path& addonFolder) const
{
    const auto known = known_.find(ComparablePath(addonFolder));

    return known == known_.end() ? std::nullopt : std::optional(known->second);
}

void JsonSceneryCache::Keep(const std::filesystem::path& addonFolder, const RememberedScenery& scenery)
{
    known_.insert_or_assign(ComparablePath(addonFolder), scenery);

    Write();
}
