#ifndef FS_ORGANIZER_DOMAIN_JOURNAL_JOURNAL_ENTRIES_H
#define FS_ORGANIZER_DOMAIN_JOURNAL_JOURNAL_ENTRIES_H

#include <vector>

#include "domain/model/OperationRecord.h"

enum class JournalEntryKind : int
{
    OneOperation = 0,
    ImportRun = 1,
    Swap = 2,
};

struct JournalEntry
{
    std::vector<OperationRecord> steps{};
    JournalEntryKind kind = JournalEntryKind::OneOperation;

    [[nodiscard]] const OperationRecord& First() const;

    [[nodiscard]] const OperationRecord& Last() const;

    [[nodiscard]] const OperationRecord& WhereItStopped() const;

    [[nodiscard]] bool IsAnImportRun() const;

    [[nodiscard]] bool IsASwap() const;

    [[nodiscard]] bool HasSteps() const;

    [[nodiscard]] bool Succeeded() const;
};

[[nodiscard]] bool StepSucceeded(const OperationRecord& record);

[[nodiscard]] std::vector<JournalEntry> GroupOperations(const std::vector<OperationRecord>& records);

#endif // FS_ORGANIZER_DOMAIN_JOURNAL_JOURNAL_ENTRIES_H
