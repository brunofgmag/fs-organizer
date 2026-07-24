#ifndef FS_ORGANIZER_DOMAIN_MODEL_DESTINATION_ENTRY_H
#define FS_ORGANIZER_DOMAIN_MODEL_DESTINATION_ENTRY_H

#include <filesystem>

#include "domain/model/EntryClassification.h"

struct DestinationEntry
{
    std::filesystem::path path;
    std::filesystem::path target;
    EntryClassification classification = EntryClassification::Unmanaged;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_DESTINATION_ENTRY_H
