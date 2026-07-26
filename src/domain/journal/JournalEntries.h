#ifndef FS_ORGANIZER_DOMAIN_JOURNAL_JOURNAL_ENTRIES_H
#define FS_ORGANIZER_DOMAIN_JOURNAL_JOURNAL_ENTRIES_H

#include <vector>

#include "domain/model/OperationRecord.h"

struct JournalEntry
{
    std::vector<OperationRecord> steps;

    [[nodiscard]] const OperationRecord& First() const;

    [[nodiscard]] const OperationRecord& Last() const;

    [[nodiscard]] bool IsAnImportRun() const;

    [[nodiscard]] bool Succeeded() const;
};

[[nodiscard]] bool StepSucceeded(const OperationRecord& record);

[[nodiscard]] std::vector<JournalEntry> GroupImportRuns(const std::vector<OperationRecord>& records);

#endif // FS_ORGANIZER_DOMAIN_JOURNAL_JOURNAL_ENTRIES_H
