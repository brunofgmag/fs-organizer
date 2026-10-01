#ifndef FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JOURNAL_IMPORTED_FOLDERS_H
#define FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JOURNAL_IMPORTED_FOLDERS_H

#include <cstddef>
#include <mutex>

#include "domain/importing/WhatTheImporterBrought.h"
#include "domain/ports/ImportedFolders.h"
#include "domain/ports/OperationJournal.h"

class JournalImportedFolders final : public ImportedFolders
{
public:
    explicit JournalImportedFolders(const OperationJournal& journal);

    [[nodiscard]] std::vector<std::filesystem::path> WhatTheImporterBrought() const override;

private:
    const OperationJournal& journal_;
    mutable std::mutex guard_;
    mutable FoldersTheImporterBroughtSoFar brought_{};
    mutable std::size_t folded_ = 0;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JOURNAL_IMPORTED_FOLDERS_H
