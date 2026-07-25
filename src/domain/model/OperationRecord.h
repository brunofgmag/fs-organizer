#ifndef FS_ORGANIZER_DOMAIN_MODEL_OPERATION_RECORD_H
#define FS_ORGANIZER_DOMAIN_MODEL_OPERATION_RECORD_H

#include <chrono>
#include <filesystem>
#include <utility>

#include "domain/model/AddonId.h"
#include "domain/model/ImportResult.h"
#include "domain/model/LinkFailure.h"
#include "domain/model/OperationKind.h"

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
        record.failure = failure;

        return record;
    }

    [[nodiscard]] static OperationRecord OfImport(std::chrono::system_clock::time_point timestamp,
                                                  OperationKind kind,
                                                  AddonId addonId,
                                                  std::filesystem::path source,
                                                  std::filesystem::path target,
                                                  ImportResult result)
    {
        OperationRecord record;
        record.timestamp = timestamp;
        record.kind = kind;
        record.addonId = std::move(addonId);
        record.source = std::move(source);
        record.target = std::move(target);
        record.importResult = result;

        return record;
    }

    std::chrono::system_clock::time_point timestamp;
    OperationKind kind = OperationKind::EnableAddon;
    AddonId addonId;
    std::filesystem::path source;
    std::filesystem::path target;
    LinkFailure failure = LinkFailure::None;
    ImportResult importResult = ImportResult::Completed;

private:
    OperationRecord() = default;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_OPERATION_RECORD_H
