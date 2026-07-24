#ifndef FS_ORGANIZER_DOMAIN_MODEL_OCCUPIED_DESTINATION_H
#define FS_ORGANIZER_DOMAIN_MODEL_OCCUPIED_DESTINATION_H

#include <filesystem>

struct OccupiedDestination
{
    std::filesystem::path DestinationPath;
    std::filesystem::path ExistingTarget;
};

#endif
