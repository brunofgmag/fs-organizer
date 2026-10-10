#ifndef FS_ORGANIZER_APPLICATION_MODEL_ADMISSION_H
#define FS_ORGANIZER_APPLICATION_MODEL_ADMISSION_H

#include <filesystem>
#include <string>

#include "domain/model/FileResult.h"

struct AdmissionCheck
{
    FileResult result = FileResult::TheOutcomeIsUnknown;
    std::filesystem::path target{};
    std::filesystem::path occupant{};
    std::string occupantVersion{};
    bool theQuarantineHoldsAnEarlierCopy = false;
    std::string quarantinedVersion{};
    std::filesystem::path quarantinedCopy{};
};

struct AdmitRequest
{
    std::filesystem::path staged{};
    std::filesystem::path category{};
    std::filesystem::path replaces{};
    bool discardTheEarlierQuarantinedCopy = false;
};

struct AdmitResult
{
    FileResult result = FileResult::TheOutcomeIsUnknown;
    std::filesystem::path landed{};
    bool replaced = false;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_ADMISSION_H
