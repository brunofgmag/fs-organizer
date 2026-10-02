#ifndef FS_ORGANIZER_TESTS_SUPPORT_CATALOGUE_BESIDE_THE_BUILD_H
#define FS_ORGANIZER_TESTS_SUPPORT_CATALOGUE_BESIDE_THE_BUILD_H

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QString>

inline QString TheCatalogueBesideTheBuild(const QString& language)
{
    const QString name = QStringLiteral("app_%1.qm").arg(language);

    for (QDir dir(QCoreApplication::applicationDirPath()); !dir.isRoot(); dir.cdUp())
    {
        if (const QString found = dir.filePath(name); QFileInfo::exists(found))
        {
            return found;
        }
    }

    return {};
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_CATALOGUE_BESIDE_THE_BUILD_H
