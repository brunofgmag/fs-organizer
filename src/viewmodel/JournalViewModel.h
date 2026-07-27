#ifndef FS_ORGANIZER_VIEWMODEL_JOURNAL_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_JOURNAL_VIEW_MODEL_H

#include <QtCore/QObject>

#include "application/Session.h"
#include "domain/ports/OperationJournal.h"
#include "viewmodel/JournalModel.h"

class JournalViewModel final : public QObject
{
    Q_OBJECT

public:
    JournalViewModel(const OperationJournal& journal,
                     const Session& session,
                     JournalModel& model,
                     QObject* parent = nullptr);

    void Show();

signals:
    void Shown(std::size_t operations);

private:
    const OperationJournal& journal_;
    const Session& session_;
    JournalModel& model_;
};

#endif // FS_ORGANIZER_VIEWMODEL_JOURNAL_VIEW_MODEL_H
