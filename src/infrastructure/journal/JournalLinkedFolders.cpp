#include "infrastructure/journal/JournalLinkedFolders.h"

JournalLinkedFolders::JournalLinkedFolders(const OperationJournal& journal) : journal_(journal)
{
}

std::vector<LinkTheAppMade> JournalLinkedFolders::WhatTheAppLinked() const
{
    const std::lock_guard lock(guard_);

    for (const OperationRecord& record : journal_.ReadFrom(folded_))
    {
        made_.Fold(record);
        ++folded_;
    }

    return made_.Links();
}
