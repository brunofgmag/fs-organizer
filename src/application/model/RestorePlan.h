#ifndef FS_ORGANIZER_APPLICATION_MODEL_RESTORE_PLAN_H
#define FS_ORGANIZER_APPLICATION_MODEL_RESTORE_PLAN_H

#include <filesystem>
#include <string>
#include <vector>

#include "application/model/QuarantinedItem.h"
#include "application/model/SizeReport.h"
#include "domain/model/FileResult.h"

struct RestoreCheck
{
    QuarantinedItem item{};
    std::filesystem::path target{};
    FileResult result = FileResult::Completed;
    std::filesystem::path occupant{};
    std::string version{};
    std::string occupantVersion{};
    bool occupantIsAnAddon = false;
    bool theOriginHoldsALink = false;

    [[nodiscard]] bool CanProceed() const
    {
        return Succeeded(result);
    }

    [[nodiscard]] bool NeedsAPlace() const
    {
        return result == FileResult::TheOriginIsUnknown;
    }

    [[nodiscard]] bool CollidesWithAnOccupant() const
    {
        return result == FileResult::TheIdentityIsTaken || result == FileResult::TheOriginIsOccupied;
    }

    [[nodiscard]] bool CanBeSwapped() const
    {
        return CollidesWithAnOccupant() && occupantIsAnAddon && !occupant.empty();
    }
};

struct TwoSides
{
    MeasuredFolder held{};
    MeasuredFolder occupant{};
};

enum class SwapStep : int
{
    QuarantineTheOccupant = 0,
    RestoreTheItem = 1,
};

struct SwapResult
{
    std::filesystem::path item{};
    std::filesystem::path occupant{};
    std::filesystem::path inTheLibrary{};
    SwapStep stoppedAt = SwapStep::QuarantineTheOccupant;
    FileResult result = FileResult::Completed;

    [[nodiscard]] bool Succeeded() const
    {
        return ::Succeeded(result);
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
