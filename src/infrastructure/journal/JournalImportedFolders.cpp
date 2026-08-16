#include "infrastructure/journal/JournalImportedFolders.h"

#include "domain/importing/WhatTheImporterBrought.h"

JournalImportedFolders::JournalImportedFolders(const OperationJournal& journal) : journal_(journal)
{
}

std::vector<std::filesystem::path> JournalImportedFolders::WhatTheImporterBrought() const
{
    return FoldersTheImporterBrought(journal_.Read());
}
