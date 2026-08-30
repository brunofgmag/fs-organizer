#ifndef FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JSONL_OPERATION_JOURNAL_H
#define FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JSONL_OPERATION_JOURNAL_H

#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>

#include "domain/ports/OperationJournal.h"

class JsonlOperationJournal final : public OperationJournal
{
public:
    explicit JsonlOperationJournal(std::filesystem::path file);

    void Append(const OperationRecord& record) override;

    [[nodiscard]] std::vector<OperationRecord> Read() const override;

private:
    [[nodiscard]] std::vector<OperationRecord> WhatTheFileHolds() const;

    std::filesystem::path file_;
    std::ofstream stream_;
    mutable std::mutex guard_;
    mutable std::optional<std::vector<OperationRecord>> known_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_JOURNAL_JSONL_OPERATION_JOURNAL_H
