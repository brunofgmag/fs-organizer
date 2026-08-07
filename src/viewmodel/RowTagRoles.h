#ifndef FS_ORGANIZER_VIEWMODEL_ROW_TAG_ROLES_H
#define FS_ORGANIZER_VIEWMODEL_ROW_TAG_ROLES_H

#include <QtCore/Qt>

enum RowTagRole : int
{
    TagTextRole = Qt::UserRole + 200,
    TagToneRole = Qt::UserRole + 201,
    AlarmingRole = Qt::UserRole + 202,
    QuietRole = Qt::UserRole + 203,
    AlertRole = Qt::UserRole + 204,
    QuietSuffixRole = Qt::UserRole + 205,
    EmphasisRole = Qt::UserRole + 206,
    SecondLineRole = Qt::UserRole + 207,
};

#endif // FS_ORGANIZER_VIEWMODEL_ROW_TAG_ROLES_H
