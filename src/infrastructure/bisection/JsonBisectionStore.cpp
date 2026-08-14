#include "infrastructure/bisection/JsonBisectionStore.h"

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
#include "support/FileWriting.h"

namespace
{
    constexpr auto kFileSuffix = ".bisection.json";
    constexpr auto kProfileId = "profileId";
    constexpr auto kUnits = "units";
    constexpr auto kAddons = "addons";
    constexpr auto kBase = "base";
    constexpr auto kCoupling = "coupling";
    constexpr auto kSuspects = "suspects";
    constexpr auto kCleared = "cleared";
    constexpr auto kAlwaysOn = "alwaysOn";
    constexpr auto kRound = "round";
    constexpr auto kPass = "pass";
    constexpr auto kTheReferenceRoundCrashed = "theReferenceRoundCrashed";
    constexpr auto kStartingConfiguration = "startingConfiguration";
    constexpr auto kStartedAt = "startedAt";
    constexpr auto kLibraryId = "libraryId";
    constexpr auto kFolderName = "folderName";
    constexpr auto kAction = "action";
    constexpr auto kInsideTheGroup = "insideTheGroup";
    constexpr auto kOverTheUnits = "overTheUnits";

    QJsonArray PathsToJson(const std::vector<std::filesystem::path>& paths)
    {
        QJsonArray array;

        for (const std::filesystem::path& path : paths)
        {
            array.append(QString::fromStdString(AsUtf8(path)));
        }

        return array;
    }

    std::vector<std::filesystem::path> PathsFromJson(const QJsonArray& array)
    {
        std::vector<std::filesystem::path> paths;

        for (const QJsonValue path : array)
        {
            paths.push_back(PathFromUtf8(path.toString().toStdString()));
        }

        return paths;
    }

    QJsonArray NumbersToJson(const std::vector<std::size_t>& numbers)
    {
        QJsonArray array;

        for (const std::size_t number : numbers)
        {
            array.append(static_cast<qint64>(number));
        }

        return array;
    }

    std::vector<std::size_t> NumbersFromJson(const QJsonArray& array)
    {
        std::vector<std::size_t> numbers;

        for (const QJsonValue number : array)
        {
            numbers.push_back(static_cast<std::size_t>(number.toInteger()));
        }

        return numbers;
    }

    QJsonObject ToJson(const SearchUnit& unit)
    {
        QJsonObject object;
        object[kAddons] = PathsToJson(unit.addons);
        object[kCoupling] = static_cast<int>(unit.coupling);

        if (unit.base.has_value())
        {
            object[kBase] = QString::fromStdString(AsUtf8(*unit.base));
        }

        return object;
    }

    SearchUnit UnitFromJson(const QJsonObject& object)
    {
        SearchUnit unit;
        unit.addons = PathsFromJson(object.value(kAddons).toArray());
        unit.coupling =
            static_cast<Coupling>(object.value(kCoupling).toInt(static_cast<int>(Coupling::NotYetMeasured)));

        if (object.contains(kBase))
        {
            unit.base = PathFromUtf8(object.value(kBase).toString().toStdString());
        }

        return unit;
    }

    QJsonArray ToJson(const std::vector<PresetEntry>& entries)
    {
        QJsonArray array;

        for (const PresetEntry& entry : entries)
        {
            QJsonObject object;
            object[kLibraryId] = QString::fromStdString(entry.addonId.libraryId);
            object[kFolderName] = QString::fromStdString(entry.addonId.folderName);
            object[kAction] = entry.action == PresetAction::Disable ? "disable" : "enable";

            array.append(object);
        }

        return array;
    }

    std::vector<PresetEntry> EntriesFromJson(const QJsonArray& array)
    {
        std::vector<PresetEntry> entries;

        for (const QJsonValue value : array)
        {
            const QJsonObject object = value.toObject();

            PresetEntry entry;
            entry.addonId.libraryId = object.value(kLibraryId).toString().toStdString();
            entry.addonId.folderName = object.value(kFolderName).toString().toStdString();
            entry.action = object.value(kAction).toString() == "disable" ? PresetAction::Disable : PresetAction::Enable;

            entries.push_back(entry);
        }

        return entries;
    }
}

JsonBisectionStore::JsonBisectionStore(std::filesystem::path root) : root_(std::move(root))
{
}

std::optional<std::filesystem::path> JsonBisectionStore::FileOf(const std::string& profileId) const
{
    const std::optional<PathSegment> segment = PathSegment::From(profileId);

    if (!segment.has_value())
    {
        return std::nullopt;
    }

    return PathUnder(root_, PathFromUtf8(segment->Text() + kFileSuffix));
}

std::optional<BisectionRun> JsonBisectionStore::Load(const std::string& profileId) const
{
    const std::optional<std::filesystem::path> file = FileOf(profileId);

    if (!file.has_value())
    {
        return std::nullopt;
    }

    std::ifstream stream(*file, std::ios::binary);
    const std::string content{std::istreambuf_iterator(stream), std::istreambuf_iterator<char>()};

    const QJsonDocument document =
        QJsonDocument::fromJson(QByteArray::fromRawData(content.data(), static_cast<qsizetype>(content.size())));

    if (!document.isObject())
    {
        return std::nullopt;
    }

    const QJsonObject root = document.object();

    BisectionRun run;
    run.profileId = root.value(kProfileId).toString().toStdString();
    run.suspects = NumbersFromJson(root.value(kSuspects).toArray());
    run.cleared = NumbersFromJson(root.value(kCleared).toArray());
    run.alwaysOn = PathsFromJson(root.value(kAlwaysOn).toArray());
    run.round = static_cast<std::size_t>(root.value(kRound).toInteger());
    run.theReferenceRoundCrashed = root.value(kTheReferenceRoundCrashed).toBool();
    run.startingConfiguration = EntriesFromJson(root.value(kStartingConfiguration).toArray());
    run.startedAt =
        std::chrono::system_clock::time_point(std::chrono::milliseconds(root.value(kStartedAt).toInteger()));

    if (root.value(kPass).toString() == kInsideTheGroup)
    {
        run.pass = BisectionPass::InsideTheGroup;
    }

    for (const QJsonValue unit : root.value(kUnits).toArray())
    {
        run.units.push_back(UnitFromJson(unit.toObject()));
    }

    return run;
}

bool JsonBisectionStore::Save(const std::string& profileId, const BisectionRun& run)
{
    const std::optional<std::filesystem::path> file = FileOf(profileId);

    if (!file.has_value())
    {
        return false;
    }

    QJsonArray units;

    for (const SearchUnit& unit : run.units)
    {
        units.append(ToJson(unit));
    }

    QJsonObject root;
    root[kProfileId] = QString::fromStdString(run.profileId);
    root[kUnits] = units;
    root[kSuspects] = NumbersToJson(run.suspects);
    root[kCleared] = NumbersToJson(run.cleared);
    root[kAlwaysOn] = PathsToJson(run.alwaysOn);
    root[kRound] = static_cast<qint64>(run.round);
    root[kPass] = run.pass == BisectionPass::InsideTheGroup ? kInsideTheGroup : kOverTheUnits;
    root[kTheReferenceRoundCrashed] = run.theReferenceRoundCrashed;
    root[kStartingConfiguration] = ToJson(run.startingConfiguration);
    root[kStartedAt] = static_cast<qint64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(run.startedAt.time_since_epoch()).count());

    std::error_code error;
    std::filesystem::create_directories(file->parent_path(), error);

    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);

    return WriteFileReplacing(*file, {json.constData(), static_cast<std::size_t>(json.size())});
}

void JsonBisectionStore::Forget(const std::string& profileId)
{
    const std::optional<std::filesystem::path> file = FileOf(profileId);

    if (!file.has_value())
    {
        return;
    }

    std::error_code error;
    std::filesystem::remove(*file, error);
}
