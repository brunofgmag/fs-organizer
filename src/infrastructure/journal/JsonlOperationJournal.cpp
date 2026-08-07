#include "infrastructure/journal/JsonlOperationJournal.h"

#include <array>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QTimeZone>

#include "support/PathText.h"

namespace
{
    constexpr auto kTimestamp = "timestamp";
    constexpr auto kKind = "kind";
    constexpr auto kLibraryId = "libraryId";
    constexpr auto kAddon = "addon";
    constexpr auto kSource = "source";
    constexpr auto kTarget = "target";
    constexpr auto kFailure = "failure";
    constexpr auto kResult = "result";

    QString KindName(const OperationKind kind)
    {
        switch (kind)
        {
        case OperationKind::EnableAddon: return "enable";
        case OperationKind::DisableAddon: return "disable";
        case OperationKind::RemoveBrokenLink: return "removeBrokenLink";
        case OperationKind::RepointLink: return "repointLink";
        case OperationKind::ImportCopyToStaging: return "importCopyToStaging";
        case OperationKind::ImportVerifyStaging: return "importVerifyStaging";
        case OperationKind::ImportMoveIntoPlace: return "importMoveIntoPlace";
        case OperationKind::ImportRemoveSource: return "importRemoveSource";
        case OperationKind::QuarantineFromDestination: return "quarantineFromDestination";
        case OperationKind::QuarantineFromLibrary: return "quarantineFromLibrary";
        case OperationKind::RestoreFromQuarantine: return "restoreFromQuarantine";
        case OperationKind::DiscardFromQuarantine: return "discardFromQuarantine";
        case OperationKind::DiscardStaging: return "discardStaging";
        case OperationKind::MoveAddon: return "moveAddon";
        case OperationKind::CreateCategory: return "createCategory";
        case OperationKind::RenameCategory: return "renameCategory";
        case OperationKind::RemoveCategory: return "removeCategory";
        case OperationKind::RecycleFromLibrary: return "recycleFromLibrary";
        case OperationKind::DeleteFromLibrary: return "deleteFromLibrary";
        }

        return "unknown";
    }

    QString FailureName(const LinkFailure failure)
    {
        switch (failure)
        {
        case LinkFailure::None: return "none";
        case LinkFailure::DestinationHoldsRealFolder: return "destinationHoldsRealFolder";
        case LinkFailure::DestinationHoldsLiveLink: return "destinationHoldsLiveLink";
        case LinkFailure::UnreadableLinkTarget: return "unreadableLinkTarget";
        case LinkFailure::CouldNotReplaceStaleLink: return "couldNotReplaceStaleLink";
        case LinkFailure::CouldNotCreateLink: return "couldNotCreateLink";
        case LinkFailure::PathIsNotAReparsePoint: return "pathIsNotAReparsePoint";
        case LinkFailure::CouldNotRemoveLink: return "couldNotRemoveLink";
        case LinkFailure::PrivilegeNotHeld: return "privilegeNotHeld";
        case LinkFailure::TheOutcomeIsUnknown: return "theOutcomeIsUnknown";
        }

        return "unknown";
    }

    QString ResultName(const FileResult result)
    {
        switch (result)
        {
        case FileResult::Completed: return "completed";
        case FileResult::Cancelled: return "cancelled";
        case FileResult::TheSimulatorIsRunning: return "theSimulatorIsRunning";
        case FileResult::CouldNotQuarantine: return "couldNotQuarantine";
        case FileResult::SourceIsNotUnderADestination: return "sourceIsNotUnderADestination";
        case FileResult::SourceIsAReparsePoint: return "sourceIsAReparsePoint";
        case FileResult::CouldNotCheckFreeSpace: return "couldNotCheckFreeSpace";
        case FileResult::NotEnoughFreeSpace: return "notEnoughFreeSpace";
        case FileResult::CouldNotCopy: return "couldNotCopy";
        case FileResult::VerificationFailed: return "verificationFailed";
        case FileResult::CouldNotMoveIntoPlace: return "couldNotMoveIntoPlace";
        case FileResult::CouldNotRemoveSource: return "couldNotRemoveSource";
        case FileResult::CouldNotCreateLink: return "couldNotCreateLink";
        case FileResult::TheOriginIsUnknown: return "theOriginIsUnknown";
        case FileResult::CouldNotRestore: return "couldNotRestore";
        case FileResult::CouldNotDiscard: return "couldNotDiscard";
        case FileResult::CouldNotRemoveTheLink: return "couldNotRemoveTheLink";
        case FileResult::TheIdentityIsTaken: return "theIdentityIsTaken";
        case FileResult::TheTargetIsNotInALibrary: return "theTargetIsNotInALibrary";
        case FileResult::CouldNotCreateTheCategory: return "couldNotCreateTheCategory";
        case FileResult::TheCategoryStillHoldsAddons: return "theCategoryStillHoldsAddons";
        case FileResult::CouldNotRemoveTheCategory: return "couldNotRemoveTheCategory";
        case FileResult::TheOutcomeIsUnknown: return "theOutcomeIsUnknown";
        case FileResult::CouldNotReadTheSource: return "couldNotReadTheSource";
        case FileResult::TheOriginIsOccupied: return "theOriginIsOccupied";
        case FileResult::TheRecycleBinIsTooSmall: return "theRecycleBinIsTooSmall";
        case FileResult::TheRecycleBinCannotReachIt: return "theRecycleBinCannotReachIt";
        case FileResult::CouldNotDelete: return "couldNotDelete";
        }

        return "unknown";
    }

    QString Moment(const std::chrono::system_clock::time_point& timestamp)
    {
        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();

        return QDateTime::fromMSecsSinceEpoch(milliseconds, QTimeZone::UTC).toString(Qt::ISODate);
    }

    std::chrono::system_clock::time_point MomentFrom(const QString& text)
    {
        QDateTime moment = QDateTime::fromString(text, Qt::ISODate);
        moment.setTimeZone(QTimeZone::UTC);

        return std::chrono::system_clock::time_point{std::chrono::milliseconds{moment.toMSecsSinceEpoch()}};
    }

    template<typename Value, std::size_t Count, typename Naming>
    std::optional<Value> ValueNamed(const std::array<Value, Count>& all, const Naming& nameOf, const QString& name)
    {
        for (const Value candidate : all)
        {
            if (nameOf(candidate) == name)
            {
                return candidate;
            }
        }

        return std::nullopt;
    }

    std::optional<OperationRecord> RecordFrom(const QJsonObject& object)
    {
        const std::optional<OperationKind> kind = ValueNamed(kAllOperationKinds, KindName, object[kKind].toString());
        if (!kind.has_value())
        {
            return std::nullopt;
        }

        const AddonId addon{.libraryId = object[kLibraryId].toString().toStdString(),
                            .folderName = object[kAddon].toString().toStdString()};
        const auto timestamp = MomentFrom(object[kTimestamp].toString());
        const std::filesystem::path source = AsPath(object[kSource].toString());
        const std::filesystem::path target = AsPath(object[kTarget].toString());

        if (object.contains(kResult))
        {
            return OperationRecord::OfImport(timestamp, *kind, addon, source, target,
                                             ValueNamed(kAllFileResults, ResultName, object[kResult].toString())
                                                 .value_or(FileResult::TheOutcomeIsUnknown));
        }

        return OperationRecord::OfLink(timestamp, *kind, addon, source, target,
                                       ValueNamed(kAllLinkFailures, FailureName, object[kFailure].toString())
                                           .value_or(LinkFailure::TheOutcomeIsUnknown));
    }
}

JsonlOperationJournal::JsonlOperationJournal(std::filesystem::path file) : file_(std::move(file))
{
}

void JsonlOperationJournal::Append(const OperationRecord& record)
{
    QJsonObject object;
    object[kTimestamp] = Moment(record.timestamp);
    object[kKind] = KindName(record.kind);
    object[kLibraryId] = QString::fromStdString(record.addonId.libraryId);
    object[kAddon] = QString::fromStdString(record.addonId.folderName);
    object[kSource] = AsText(record.source);
    object[kTarget] = AsText(record.target);

    if (const FileResult* result = std::get_if<FileResult>(&record.outcome))
    {
        object[kResult] = ResultName(*result);
    }
    else
    {
        object[kFailure] = FailureName(std::get<LinkFailure>(record.outcome));
    }

    if (!stream_.is_open())
    {
        std::error_code error;
        std::filesystem::create_directories(file_.parent_path(), error);
        stream_.open(file_, std::ios::binary | std::ios::app);
    }

    const QByteArray line = QJsonDocument(object).toJson(QJsonDocument::Compact);
    stream_.write(line.constData(), line.size());
    stream_.put('\n');
    stream_.flush();
}

std::vector<OperationRecord> JsonlOperationJournal::Read() const
{
    std::vector<OperationRecord> records;

    std::ifstream file(file_, std::ios::binary);
    for (std::string line; std::getline(file, line);)
    {
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(line));
        if (!document.isObject())
        {
            continue;
        }

        if (const std::optional<OperationRecord> record = RecordFrom(document.object()))
        {
            records.push_back(*record);
        }
    }

    return records;
}
