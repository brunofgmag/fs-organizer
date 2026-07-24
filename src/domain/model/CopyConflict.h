#ifndef FS_ORGANIZER_DOMAIN_MODEL_COPY_CONFLICT_H
#define FS_ORGANIZER_DOMAIN_MODEL_COPY_CONFLICT_H

#include <filesystem>

struct CopyConflict
{
    std::filesystem::path destinationPath;
    std::filesystem::path libraryPath;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_COPY_CONFLICT_H
