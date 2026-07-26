#ifndef FS_ORGANIZER_SUPPORT_SIZE_TEXT_H
#define FS_ORGANIZER_SUPPORT_SIZE_TEXT_H

#include <cstdint>

#include <QtCore/QLocale>
#include <QtCore/QString>

[[nodiscard]] inline QString AsSize(const std::uintmax_t bytes)
{
    return QLocale().formattedDataSize(static_cast<qint64>(bytes));
}

#endif // FS_ORGANIZER_SUPPORT_SIZE_TEXT_H
