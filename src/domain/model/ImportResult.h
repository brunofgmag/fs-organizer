#ifndef FS_ORGANIZER_DOMAIN_MODEL_IMPORT_RESULT_H
#define FS_ORGANIZER_DOMAIN_MODEL_IMPORT_RESULT_H

enum class ImportResult
{
    Completed,
    Cancelled,
    SourceIsNotUnderADestination,
    SourceIsAReparsePoint,
    CouldNotCheckFreeSpace,
    NotEnoughFreeSpace,
    CouldNotCopy,
    VerificationFailed,
    CouldNotMoveIntoPlace,
    CouldNotRemoveSource
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_IMPORT_RESULT_H
