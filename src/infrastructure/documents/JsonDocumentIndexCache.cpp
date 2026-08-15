#include "infrastructure/documents/JsonDocumentIndexCache.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
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
    constexpr auto kReadAt = "readAt";
    constexpr auto kLibrary = "library";
    constexpr auto kFolderName = "folderName";
    constexpr auto kFolder = "folder";
    constexpr auto kItWasWalked = "itWasWalked";
    constexpr auto kDocuments = "documents";
    constexpr auto kAirports = "airports";
    constexpr auto kCode = "code";
    constexpr auto kCatalogued = "catalogued";
    constexpr auto kEntries = "entries";
    constexpr auto kTypes = "types";
    constexpr auto kType = "type";
    constexpr auto kCharts = "charts";
    constexpr auto kName = "name";
    constexpr auto kRevision = "revision";
    constexpr auto kPreviousRevision = "previous";
    constexpr auto kPages = "pages";

    [[nodiscard]] qint64 MillisecondsOf(const std::chrono::system_clock::time_point moment)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(moment.time_since_epoch()).count();
    }

    [[nodiscard]] std::chrono::system_clock::time_point MomentOf(const qint64 milliseconds)
    {
        return std::chrono::system_clock::time_point{std::chrono::milliseconds{milliseconds}};
    }

    [[nodiscard]] QJsonArray PathsToJson(const std::vector<std::filesystem::path>& paths)
    {
        QJsonArray written;

        for (const std::filesystem::path& path : paths)
        {
            written.append(QString::fromStdString(AsUtf8(path)));
        }

        return written;
    }

    [[nodiscard]] std::vector<std::filesystem::path> PathsFromJson(const QJsonArray& read)
    {
        std::vector<std::filesystem::path> paths;

        for (const QJsonValue& path : read)
        {
            paths.push_back(PathFromUtf8(path.toString().toStdString()));
        }

        return paths;
    }

    [[nodiscard]] QJsonObject ToJson(const ChartEntry& chart)
    {
        QJsonObject object;
        object[kName] = QString::fromStdString(chart.name);
        object[kRevision] = chart.revision == ChartRevision::Previous ? QString::fromUtf8(kPreviousRevision)
                                                                      : QStringLiteral("inForce");
        object[kPages] = PathsToJson(chart.pages);

        return object;
    }

    [[nodiscard]] ChartEntry ChartFromJson(const QJsonObject& object)
    {
        return {.name = object.value(kName).toString().toStdString(),
                .revision = object.value(kRevision).toString() == QString::fromUtf8(kPreviousRevision)
                    ? ChartRevision::Previous
                    : ChartRevision::InForce,
                .pages = PathsFromJson(object.value(kPages).toArray())};
    }

    [[nodiscard]] QJsonObject ToJson(const ChartsOfAnAirport& airport)
    {
        QJsonArray types;

        for (const ChartsOfAType& group : airport.types)
        {
            QJsonArray charts;
            for (const ChartEntry& chart : group.charts)
            {
                charts.append(ToJson(chart));
            }

            QJsonObject written;
            written[kType] = QString::fromStdString(group.type);
            written[kCharts] = charts;

            types.append(written);
        }

        QJsonObject object;
        object[kCode] = QString::fromStdString(airport.code);
        object[kCatalogued] = airport.catalogued;
        object[kEntries] = static_cast<double>(airport.entriesInTheCatalogue);
        object[kTypes] = types;

        return object;
    }

    [[nodiscard]] ChartsOfAnAirport AirportFromJson(const QJsonObject& object)
    {
        ChartsOfAnAirport airport{.code = object.value(kCode).toString().toStdString(),
                                  .catalogued = object.value(kCatalogued).toBool(),
                                  .entriesInTheCatalogue = static_cast<std::size_t>(object.value(kEntries).toDouble()),
                                  .types = {}};

        for (const QJsonValue& value : object.value(kTypes).toArray())
        {
            const QJsonObject read = value.toObject();

            ChartsOfAType group{.type = read.value(kType).toString().toStdString(), .charts = {}};

            for (const QJsonValue& chart : read.value(kCharts).toArray())
            {
                group.charts.push_back(ChartFromJson(chart.toObject()));
            }

            airport.types.push_back(std::move(group));
        }

        return airport;
    }

    [[nodiscard]] QJsonObject ToJson(const DocumentsOfAnAddon& addon)
    {
        QJsonArray airports;

        for (const ChartsOfAnAirport& airport : addon.airports)
        {
            airports.append(ToJson(airport));
        }

        QJsonObject object;
        object[kLibrary] = QString::fromStdString(addon.addon.libraryId);
        object[kFolderName] = QString::fromStdString(addon.addon.folderName);
        object[kFolder] = QString::fromStdString(AsUtf8(addon.folder));
        object[kItWasWalked] = addon.itWasWalked;
        object[kDocuments] = PathsToJson(addon.documents);
        object[kAirports] = airports;

        return object;
    }

    [[nodiscard]] DocumentsOfAnAddon AddonFromJson(const QJsonObject& object)
    {
        DocumentsOfAnAddon addon{.addon = {.libraryId = object.value(kLibrary).toString().toStdString(),
                                           .folderName = object.value(kFolderName).toString().toStdString()},
                                 .folder = PathFromUtf8(object.value(kFolder).toString().toStdString()),
                                 .itWasWalked = object.value(kItWasWalked).toBool(),
                                 .documents = PathsFromJson(object.value(kDocuments).toArray()),
                                 .airports = {}};

        for (const QJsonValue& airport : object.value(kAirports).toArray())
        {
            addon.airports.push_back(AirportFromJson(airport.toObject()));
        }

        return addon;
    }
}

JsonDocumentIndexCache::JsonDocumentIndexCache(std::filesystem::path filePath) : filePath_(std::move(filePath))
{
}

std::optional<RememberedDocuments> JsonDocumentIndexCache::Remember() const
{
    std::ifstream stream(filePath_, std::ios::binary);

    if (!stream.is_open())
    {
        return std::nullopt;
    }

    const std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    const QJsonDocument document =
        QJsonDocument::fromJson(QByteArray::fromRawData(bytes.data(), static_cast<qsizetype>(bytes.size())));

    const QJsonObject root = document.object();

    if (!root.contains(QString::fromUtf8(kAddons)))
    {
        return std::nullopt;
    }

    RememberedDocuments index{.readAt = MomentOf(static_cast<qint64>(root.value(kReadAt).toDouble())), .addons = {}};

    for (const QJsonValue& addon : root.value(kAddons).toArray())
    {
        index.addons.push_back(AddonFromJson(addon.toObject()));
    }

    return index;
}

void JsonDocumentIndexCache::Keep(const RememberedDocuments& index)
{
    QJsonArray addons;

    for (const DocumentsOfAnAddon& addon : index.addons)
    {
        addons.append(ToJson(addon));
    }

    QJsonObject root;
    root[kReadAt] = static_cast<double>(MillisecondsOf(index.readAt));
    root[kAddons] = addons;

    std::error_code failed;
    std::filesystem::create_directories(ParentOf(filePath_), failed);

    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);

    static_cast<void>(
        WriteFileReplacing(filePath_, std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size()))));
}
