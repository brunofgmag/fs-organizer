#ifndef FS_ORGANIZER_DOMAIN_MODEL_IMPORT_RESULT_H
#define FS_ORGANIZER_DOMAIN_MODEL_IMPORT_RESULT_H

#include <array>
#include <cstddef>

enum class ImportResult : int
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
};

inline constexpr std::array kAllImportResults{
    ImportResult::Completed,
    ImportResult::Cancelled,
    ImportResult::TheSimulatorIsRunning,
    ImportResult::CouldNotQuarantine,
    ImportResult::SourceIsNotUnderADestination,
    ImportResult::SourceIsAReparsePoint,
    ImportResult::CouldNotCheckFreeSpace,
    ImportResult::NotEnoughFreeSpace,
    ImportResult::CouldNotCopy,
    ImportResult::VerificationFailed,
    ImportResult::CouldNotMoveIntoPlace,
    ImportResult::CouldNotRemoveSource,
    ImportResult::CouldNotCreateLink,
    ImportResult::TheOriginIsUnknown,
    ImportResult::CouldNotRestore,
    ImportResult::CouldNotDiscard,
    ImportResult::CouldNotRemoveTheLink,
};

static_assert(kAllImportResults.size() == static_cast<std::size_t>(ImportResult::CouldNotRemoveTheLink) + 1,
              "Every ImportResult belongs in kAllImportResults, and the last one carries the highest value.");

#endif // FS_ORGANIZER_DOMAIN_MODEL_IMPORT_RESULT_H
