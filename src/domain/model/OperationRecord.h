#ifndef FS_ORGANIZER_DOMAIN_MODEL_OPERATION_RECORD_H
#define FS_ORGANIZER_DOMAIN_MODEL_OPERATION_RECORD_H

#include <chrono>
#include <filesystem>

#include "domain/model/AddonId.h"
#include "domain/model/LinkFailure.h"
#include "domain/model/OperationKind.h"

struct OperationRecord
{
    std::chrono::system_clock::time_point timestamp;
    OperationKind kind = OperationKind::EnableAddon;
    AddonId addonId;
    std::filesystem::path source;
    std::filesystem::path target;
    LinkFailure failure = LinkFailure::None;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_OPERATION_RECORD_H
