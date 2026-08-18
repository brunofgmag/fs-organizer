#ifndef FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JOURNAL_LINKED_FOLDERS_H
#define FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JOURNAL_LINKED_FOLDERS_H

#include "domain/ports/LinkedFolders.h"
#include "domain/ports/OperationJournal.h"

class JournalLinkedFolders final : public LinkedFolders
{
public:
    explicit JournalLinkedFolders(const OperationJournal& journal);

    [[nodiscard]] std::vector<LinkTheAppMade> WhatTheAppLinked() const override;

private:
    const OperationJournal& journal_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JOURNAL_LINKED_FOLDERS_H
