#ifndef FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JOURNAL_IMPORTED_FOLDERS_H
#define FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JOURNAL_IMPORTED_FOLDERS_H

#include "domain/ports/ImportedFolders.h"
#include "domain/ports/OperationJournal.h"

class JournalImportedFolders final : public ImportedFolders
{
public:
    explicit JournalImportedFolders(const OperationJournal& journal);

    [[nodiscard]] std::vector<std::filesystem::path> WhatTheImporterBrought() const override;

private:
    const OperationJournal& journal_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JOURNAL_IMPORTED_FOLDERS_H
