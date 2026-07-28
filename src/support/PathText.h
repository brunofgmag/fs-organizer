#ifndef FS_ORGANIZER_SUPPORT_PATH_TEXT_H
#define FS_ORGANIZER_SUPPORT_PATH_TEXT_H

#include <filesystem>

#include <QtCore/QString>

[[nodiscard]] inline QString AsText(const std::filesystem::path& path)
{
    std::filesystem::path shown = path;
    shown.make_preferred();

    return QString::fromStdWString(shown.wstring());
}

[[nodiscard]] inline std::filesystem::path AsPath(const QString& text)
{
    return {text.toStdWString()};
}

#endif // FS_ORGANIZER_SUPPORT_PATH_TEXT_H
