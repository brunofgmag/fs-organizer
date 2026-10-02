#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_OPERATION_JOURNAL_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_OPERATION_JOURNAL_H

#include <algorithm>
#include <cstddef>
#include <vector>

#include "domain/ports/OperationJournal.h"

class FakeOperationJournal final : public OperationJournal
{
public:
    void Append(const OperationRecord& record) override
    {
        appended.push_back(record);
    }

    [[nodiscard]] std::vector<OperationRecord> Read() const override
    {
        recordsHandedOut += appended.size();

        return appended;
    }

    [[nodiscard]] std::vector<OperationRecord> ReadFrom(const std::size_t first) const override
    {
        const std::size_t from = std::min(first, appended.size());
        recordsHandedOut += appended.size() - from;

        return {appended.begin() + static_cast<std::ptrdiff_t>(from), appended.end()};
    }

    std::vector<OperationRecord> appended;
    mutable std::size_t recordsHandedOut = 0;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_OPERATION_JOURNAL_H
