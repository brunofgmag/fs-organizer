#include "viewmodel/JournalViewModel.h"

JournalViewModel::JournalViewModel(const OperationJournal& journal,
                                   const Session& session,
                                   JournalModel& model,
                                   QObject* parent)
    : QObject(parent), journal_(journal), session_(session), model_(model)
{
}

void JournalViewModel::Show()
{
    const std::vector<OperationRecord> records = journal_.Read();

    model_.ShowRecords(records, session_.Profile());

    emit Shown(records.size());
}
