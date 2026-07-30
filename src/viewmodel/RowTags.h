#ifndef FS_ORGANIZER_VIEWMODEL_ROW_TAGS_H
#define FS_ORGANIZER_VIEWMODEL_ROW_TAGS_H

#include <QtCore/Qt>

enum class TagTone : int
{
    Filled = 0,
    Outlined = 1,
    Muted = 2,
    Line = 3,
};

enum RowTagRole : int
{
    TagTextRole = Qt::UserRole + 200,
    TagToneRole = Qt::UserRole + 201,
    AlarmingRole = Qt::UserRole + 202,
    QuietRole = Qt::UserRole + 203,
    AlertRole = Qt::UserRole + 204,
    QuietSuffixRole = Qt::UserRole + 205,
    EmphasisRole = Qt::UserRole + 206,
};

#endif // FS_ORGANIZER_VIEWMODEL_ROW_TAGS_H
