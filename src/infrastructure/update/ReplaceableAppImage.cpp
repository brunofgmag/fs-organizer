#include "infrastructure/update/ReplaceableAppImage.h"

#include <QtCore/QFileInfo>

QString ReplaceableAppImage(const QString& appImageVariable)
{
    if (appImageVariable.isEmpty())
    {
        return {};
    }

    const QFileInfo image(appImageVariable);
    if (!image.isFile())
    {
        return {};
    }

    const QString resolved = image.canonicalFilePath();
    const QFileInfo folder(QFileInfo(resolved).absolutePath());

    return folder.isDir() && folder.isWritable() ? resolved : QString{};
}
