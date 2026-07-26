#ifndef FS_ORGANIZER_SUPPORT_PATH_TEXT_H
#define FS_ORGANIZER_SUPPORT_PATH_TEXT_H

#include <filesystem>

#include <QtCore/QString>

[[nodiscard]] inline QString AsText(const std::filesystem::path& path)
{
    return QString::fromStdWString(path.wstring());
}

[[nodiscard]] inline std::filesystem::path AsPath(const QString& text)
{
    return {text.toStdWString()};
}

#endif // FS_ORGANIZER_SUPPORT_PATH_TEXT_H
