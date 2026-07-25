#ifndef FS_ORGANIZER_APPLICATION_MODEL_LINK_OPERATION_RESULT_H
#define FS_ORGANIZER_APPLICATION_MODEL_LINK_OPERATION_RESULT_H

#include <filesystem>

#include "domain/model/AddonId.h"
#include "domain/model/LinkOutcome.h"
#include "domain/model/OperationKind.h"

struct LinkOperationResult
{
    AddonId addonId;
    std::filesystem::path addonFolder;
    std::filesystem::path linkPath;
    OperationKind kind = OperationKind::EnableAddon;
    LinkOutcome outcome = LinkOutcome::Success();
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_LINK_OPERATION_RESULT_H
