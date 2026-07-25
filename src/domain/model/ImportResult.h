#ifndef FS_ORGANIZER_DOMAIN_MODEL_IMPORT_RESULT_H
#define FS_ORGANIZER_DOMAIN_MODEL_IMPORT_RESULT_H

enum class ImportResult
{
    Completed,
    Cancelled,
    CouldNotCheckFreeSpace,
    NotEnoughFreeSpace,
    CouldNotCopy
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_IMPORT_RESULT_H
