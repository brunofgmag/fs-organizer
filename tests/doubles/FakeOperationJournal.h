#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_OPERATION_JOURNAL_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_OPERATION_JOURNAL_H

#include <vector>

#include "domain/ports/OperationJournal.h"

class FakeOperationJournal final : public OperationJournal
{
public:
    void Append(const OperationRecord& record) override
    {
        appended.push_back(record);
    }

    std::vector<OperationRecord> appended;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_OPERATION_JOURNAL_H
