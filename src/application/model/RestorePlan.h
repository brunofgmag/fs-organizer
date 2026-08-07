#ifndef FS_ORGANIZER_APPLICATION_MODEL_RESTORE_PLAN_H
#define FS_ORGANIZER_APPLICATION_MODEL_RESTORE_PLAN_H

#include <filesystem>
#include <string>
#include <vector>

#include "application/model/QuarantinedItem.h"
#include "domain/model/FileResult.h"

struct RestoreCheck
{
    QuarantinedItem item{};
    std::filesystem::path target{};
    FileResult result = FileResult::Completed;
    std::filesystem::path occupant{};
    std::string version{};
    std::string occupantVersion{};

    [[nodiscard]] bool CanProceed() const
    {
        return Succeeded(result);
    }

    [[nodiscard]] bool NeedsAPlace() const
    {
        return result == FileResult::TheOriginIsUnknown;
    }
};

struct RestorePlace
{
    std::filesystem::path place{};
    std::filesystem::path target{};
    std::filesystem::path label{};
};

struct RestoreOffer
{
    RestoreCheck check{};
    std::vector<RestorePlace> places{};
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_RESTORE_PLAN_H
