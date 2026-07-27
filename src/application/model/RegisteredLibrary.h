#ifndef FS_ORGANIZER_APPLICATION_MODEL_REGISTERED_LIBRARY_H
#define FS_ORGANIZER_APPLICATION_MODEL_REGISTERED_LIBRARY_H

#include <cstddef>

#include "domain/model/Library.h"

struct RegisteredLibrary
{
    Library library;
    std::size_t categories = 0;
    std::size_t addons = 0;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_REGISTERED_LIBRARY_H
