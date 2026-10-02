#ifndef FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JOURNAL_LINKED_FOLDERS_H
#define FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JOURNAL_LINKED_FOLDERS_H

#include <cstddef>
#include <mutex>

#include "domain/journal/LinksTheAppMade.h"
#include "domain/ports/LinkedFolders.h"
#include "domain/ports/OperationJournal.h"

class JournalLinkedFolders final : public LinkedFolders
{
public:
    explicit JournalLinkedFolders(const OperationJournal& journal);

    [[nodiscard]] std::vector<LinkTheAppMade> WhatTheAppLinked() const override;

private:
    const OperationJournal& journal_;
    mutable std::mutex guard_;
    mutable LinksTheAppMadeSoFar made_{};
    mutable std::size_t folded_ = 0;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JOURNAL_LINKED_FOLDERS_H
