#ifndef FS_ORGANIZER_DOMAIN_MODEL_FILE_RESULT_H
#define FS_ORGANIZER_DOMAIN_MODEL_FILE_RESULT_H

#include <array>
#include <cstddef>

enum class FileResult : int
{
    Completed = 0,
    Cancelled = 1,
    TheSimulatorIsRunning = 2,
    CouldNotQuarantine = 3,
    SourceIsNotUnderADestination = 4,
    SourceIsAReparsePoint = 5,
    CouldNotCheckFreeSpace = 6,
    NotEnoughFreeSpace = 7,
    CouldNotCopy = 8,
    VerificationFailed = 9,
    CouldNotMoveIntoPlace = 10,
    CouldNotRemoveSource = 11,
    CouldNotCreateLink = 12,
    TheOriginIsUnknown = 13,
    CouldNotRestore = 14,
    CouldNotDiscard = 15,
    CouldNotRemoveTheLink = 16,
    TheIdentityIsTaken = 17,
    TheTargetIsNotInALibrary = 18,
    CouldNotCreateTheCategory = 19,
    TheCategoryStillHoldsAddons = 20,
    CouldNotRemoveTheCategory = 21,
    TheOutcomeIsUnknown = 22,
    CouldNotReadTheSource = 23,
    TheOriginIsOccupied = 24,
    TheRecycleBinIsTooSmall = 25,
    TheRecycleBinCannotReachIt = 26,
    CouldNotDelete = 27,
    CouldNotRecordTheOrigin = 28,
    CannotWriteInTheOtherProgramsFolder = 29,
    TheDiskDisagreesWithTheScan = 30,
    CouldNotReadTheStartupFile = 31,
    CouldNotWriteTheStartupFile = 32,
};

inline constexpr std::array kAllFileResults{
    FileResult::Completed,
    FileResult::Cancelled,
    FileResult::TheSimulatorIsRunning,
    FileResult::CouldNotQuarantine,
    FileResult::SourceIsNotUnderADestination,
    FileResult::SourceIsAReparsePoint,
    FileResult::CouldNotCheckFreeSpace,
    FileResult::NotEnoughFreeSpace,
    FileResult::CouldNotCopy,
    FileResult::VerificationFailed,
    FileResult::CouldNotMoveIntoPlace,
    FileResult::CouldNotRemoveSource,
    FileResult::CouldNotCreateLink,
    FileResult::TheOriginIsUnknown,
    FileResult::CouldNotRestore,
    FileResult::CouldNotDiscard,
    FileResult::CouldNotRemoveTheLink,
    FileResult::TheIdentityIsTaken,
    FileResult::TheTargetIsNotInALibrary,
    FileResult::CouldNotCreateTheCategory,
    FileResult::TheCategoryStillHoldsAddons,
    FileResult::CouldNotRemoveTheCategory,
    FileResult::TheOutcomeIsUnknown,
    FileResult::CouldNotReadTheSource,
    FileResult::TheOriginIsOccupied,
    FileResult::TheRecycleBinIsTooSmall,
    FileResult::TheRecycleBinCannotReachIt,
    FileResult::CouldNotDelete,
    FileResult::CouldNotRecordTheOrigin,
    FileResult::CannotWriteInTheOtherProgramsFolder,
    FileResult::TheDiskDisagreesWithTheScan,
    FileResult::CouldNotReadTheStartupFile,
    FileResult::CouldNotWriteTheStartupFile,
};

static_assert(kAllFileResults.size() == static_cast<std::size_t>(FileResult::CouldNotWriteTheStartupFile) + 1,
              "Every FileResult belongs in kAllFileResults, and the last one carries the highest value.");

[[nodiscard]] constexpr bool Succeeded(const FileResult result)
{
    return result == FileResult::Completed;
}

[[nodiscard]] constexpr bool TheFolderLanded(const FileResult result)
{
    return Succeeded(result) || result == FileResult::CouldNotCreateLink;
}

#endif // FS_ORGANIZER_DOMAIN_MODEL_FILE_RESULT_H
