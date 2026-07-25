#ifndef FS_ORGANIZER_DOMAIN_MODEL_DESTINATION_OVERRIDE_H
#define FS_ORGANIZER_DOMAIN_MODEL_DESTINATION_OVERRIDE_H

#include <filesystem>

#include "domain/model/LibraryId.h"

struct DestinationOverride
{
    LibraryId libraryId;
    std::filesystem::path relativePath;
    std::filesystem::path destination;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_DESTINATION_OVERRIDE_H
