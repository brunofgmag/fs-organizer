#ifndef FS_ORGANIZER_VIEWMODEL_MODEL_RETRANSLATION_H
#define FS_ORGANIZER_VIEWMODEL_MODEL_RETRANSLATION_H

#include <QtCore/QAbstractItemModel>

inline void SayTheModelWasRetranslated(QAbstractItemModel& model)
{
    emit model.layoutAboutToBeChanged();
    emit model.layoutChanged();
}

#endif // FS_ORGANIZER_VIEWMODEL_MODEL_RETRANSLATION_H
