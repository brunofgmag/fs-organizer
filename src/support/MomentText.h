#ifndef FS_ORGANIZER_SUPPORT_MOMENT_TEXT_H
#define FS_ORGANIZER_SUPPORT_MOMENT_TEXT_H

#include <chrono>

#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QTimeZone>

inline constexpr auto kDayFormat = "dd/MM/yyyy";

[[nodiscard]] inline QDateTime AsLocalTime(const std::chrono::system_clock::time_point& timestamp)
{
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();

    return QDateTime::fromMSecsSinceEpoch(milliseconds, QTimeZone::UTC).toLocalTime();
}

[[nodiscard]] inline QString AsDay(const std::chrono::system_clock::time_point& timestamp)
{
    return AsLocalTime(timestamp).toString(QLatin1String(kDayFormat));
}

[[nodiscard]] inline QString AsMoment(const std::chrono::system_clock::time_point& timestamp)
{
    return AsLocalTime(timestamp).toString(QLatin1String(kDayFormat) + QLatin1String(" HH:mm:ss"));
}

#endif // FS_ORGANIZER_SUPPORT_MOMENT_TEXT_H
