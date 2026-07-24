#ifndef FS_ORGANIZER_DOMAIN_MODEL_ENTRY_CLASSIFICATION_H
#define FS_ORGANIZER_DOMAIN_MODEL_ENTRY_CLASSIFICATION_H

enum class EntryClassification : int
{
    Managed = 0,
    External = 1,
    Broken = 2,
    Unavailable = 3,
    Unmanaged = 4,
    Duplicated = 5,
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_ENTRY_CLASSIFICATION_H
