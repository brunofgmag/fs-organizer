#include "infrastructure/journal/JournalLinkedFolders.h"

JournalLinkedFolders::JournalLinkedFolders(const OperationJournal& journal) : journal_(journal)
{
}

std::vector<LinkTheAppMade> JournalLinkedFolders::WhatTheAppLinked() const
{
    return WhereTheAppMadeLinks(journal_.Read());
}
