#include "viewmodel/JournalViewModel.h"

JournalViewModel::JournalViewModel(const OperationJournal& journal,
                                   const AddonTreeModel& treeModel,
                                   JournalModel& model,
                                   QObject* parent)
    : QObject(parent), journal_(journal), treeModel_(treeModel), model_(model)
{
}

void JournalViewModel::Show()
{
    const std::vector<OperationRecord> records = journal_.Read();

    model_.ShowRecords(records, treeModel_.Profile());

    emit Shown(records.size());
}
