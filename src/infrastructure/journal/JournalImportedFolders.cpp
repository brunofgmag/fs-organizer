#include "infrastructure/journal/JournalImportedFolders.h"

JournalImportedFolders::JournalImportedFolders(const OperationJournal& journal) : journal_(journal)
{
}

std::vector<std::filesystem::path> JournalImportedFolders::WhatTheImporterBrought() const
{
    const std::lock_guard lock(guard_);

    for (const OperationRecord& record : journal_.ReadFrom(folded_))
    {
        brought_.Fold(record);
        ++folded_;
    }

    return brought_.Folders();
}
