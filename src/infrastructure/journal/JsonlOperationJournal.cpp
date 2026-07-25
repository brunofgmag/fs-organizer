#include "infrastructure/journal/JsonlOperationJournal.h"

#include <fstream>
#include <system_error>
#include <utility>

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QTimeZone>

namespace
{
    constexpr auto kTimestamp = "timestamp";
    constexpr auto kKind = "kind";
    constexpr auto kLibraryId = "libraryId";
    constexpr auto kAddon = "addon";
    constexpr auto kSource = "source";
    constexpr auto kTarget = "target";
    constexpr auto kFailure = "failure";

    QString FromPath(const std::filesystem::path& path)
    {
        return QString::fromStdWString(path.wstring());
    }

    QString KindName(const OperationKind kind)
    {
        switch (kind)
        {
        case OperationKind::EnableAddon: return "enable";
        case OperationKind::DisableAddon: return "disable";
        case OperationKind::RemoveBrokenLink: return "removeBrokenLink";
        case OperationKind::RepointLink: return "repointLink";
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
        }

        return "unknown";
    }

    QString Moment(const std::chrono::system_clock::time_point& timestamp)
    {
        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();

        return QDateTime::fromMSecsSinceEpoch(milliseconds, QTimeZone::UTC).toString(Qt::ISODate);
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
    object[kSource] = FromPath(record.source);
    object[kTarget] = FromPath(record.target);
    object[kFailure] = FailureName(record.failure);

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
