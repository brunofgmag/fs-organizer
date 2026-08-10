#ifndef FS_ORGANIZER_APPLICATION_MODEL_LINK_OPERATION_RESULT_H
#define FS_ORGANIZER_APPLICATION_MODEL_LINK_OPERATION_RESULT_H

#include <filesystem>
#include <optional>

#include "domain/model/AddonId.h"
#include "domain/model/FileResult.h"
#include "domain/model/LinkOutcome.h"
#include "domain/model/OperationKind.h"

struct LinkOperationResult
{
    AddonId addonId{};
    std::filesystem::path addonFolder{};
    std::filesystem::path linkPath{};
    OperationKind kind = OperationKind::EnableAddon;
    LinkOutcome outcome = LinkOutcome::Success();
    std::optional<FileResult> fileResult{};

    [[nodiscard]] bool Worked() const
    {
        return fileResult.has_value() ? Succeeded(*fileResult) : outcome.Succeeded();
    }
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_LINK_OPERATION_RESULT_H
