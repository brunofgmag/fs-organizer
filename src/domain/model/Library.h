#ifndef FS_ORGANIZER_DOMAIN_MODEL_LIBRARY_H
#define FS_ORGANIZER_DOMAIN_MODEL_LIBRARY_H

#include <filesystem>
#include <string>

#include "domain/model/LibraryId.h"

struct Library
{
    LibraryId id;
    std::filesystem::path path;
    std::string label;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_LIBRARY_H
