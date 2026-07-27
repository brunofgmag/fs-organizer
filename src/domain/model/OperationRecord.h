#ifndef FS_ORGANIZER_DOMAIN_MODEL_OPERATION_RECORD_H
#define FS_ORGANIZER_DOMAIN_MODEL_OPERATION_RECORD_H

#include <chrono>
#include <filesystem>
#include <utility>
#include <variant>

#include "domain/model/AddonId.h"
#include "domain/model/FileResult.h"
#include "domain/model/LinkFailure.h"
#include "domain/model/OperationKind.h"

using OperationOutcome = std::variant<LinkFailure, FileResult>;

[[nodiscard]] inline bool Succeeded(const OperationOutcome& outcome)
{
    if (const LinkFailure* failure = std::get_if<LinkFailure>(&outcome))
    {
        return *failure == LinkFailure::None;
    }

    return std::get<FileResult>(outcome) == FileResult::Completed;
}

struct OperationRecord
{
    [[nodiscard]] static OperationRecord OfLink(std::chrono::system_clock::time_point timestamp,
                                                OperationKind kind,
                                                AddonId addonId,
                                                std::filesystem::path source,
                                                std::filesystem::path target,
                                                LinkFailure failure)
    {
        OperationRecord record;
        record.timestamp = timestamp;
        record.kind = kind;
        record.addonId = std::move(addonId);
        record.source = std::move(source);
        record.target = std::move(target);
        record.outcome = failure;

        return record;
    }

    [[nodiscard]] static OperationRecord OfImport(std::chrono::system_clock::time_point timestamp,
                                                  OperationKind kind,
                                                  AddonId addonId,
                                                  std::filesystem::path source,
                                                  std::filesystem::path target,
                                                  FileResult result)
    {
        OperationRecord record;
        record.timestamp = timestamp;
        record.kind = kind;
        record.addonId = std::move(addonId);
        record.source = std::move(source);
        record.target = std::move(target);
        record.outcome = result;

        return record;
    }

    std::chrono::system_clock::time_point timestamp;
    OperationKind kind = OperationKind::EnableAddon;
    AddonId addonId;
    std::filesystem::path source;
    std::filesystem::path target;
    OperationOutcome outcome = LinkFailure::None;

private:
    OperationRecord() = default;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_OPERATION_RECORD_H
