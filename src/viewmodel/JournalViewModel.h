#ifndef FS_ORGANIZER_VIEWMODEL_JOURNAL_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_JOURNAL_VIEW_MODEL_H

#include <QtCore/QObject>

#include "domain/ports/OperationJournal.h"
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/JournalModel.h"

class JournalViewModel final : public QObject
{
    Q_OBJECT

public:
    JournalViewModel(const OperationJournal& journal,
                     const AddonTreeModel& treeModel,
                     JournalModel& model,
                     QObject* parent = nullptr);

    void Show();

signals:
    void Shown(std::size_t operations);

private:
    const OperationJournal& journal_;
    const AddonTreeModel& treeModel_;
    JournalModel& model_;
};

#endif // FS_ORGANIZER_VIEWMODEL_JOURNAL_VIEW_MODEL_H
