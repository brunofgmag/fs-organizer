#ifndef FS_ORGANIZER_SUPPORT_MENU_TEXT_H
#define FS_ORGANIZER_SUPPORT_MENU_TEXT_H

#include <QtCore/QString>

[[nodiscard]] inline QString AsMenuText(const QString& label)
{
    QString text = label;

    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

#endif // FS_ORGANIZER_SUPPORT_MENU_TEXT_H
