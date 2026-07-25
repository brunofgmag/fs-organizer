#ifndef FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JSONL_OPERATION_JOURNAL_H
#define FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JSONL_OPERATION_JOURNAL_H

#include <filesystem>
#include <fstream>

#include "application/ports/OperationJournal.h"

class JsonlOperationJournal final : public OperationJournal
{
public:
    explicit JsonlOperationJournal(std::filesystem::path file);

    void Append(const OperationRecord& record) override;

private:
    std::filesystem::path file_;
    std::ofstream stream_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JSONL_OPERATION_JOURNAL_H
