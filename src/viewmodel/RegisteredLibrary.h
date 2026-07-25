#ifndef FS_ORGANIZER_VIEWMODEL_REGISTERED_LIBRARY_H
#define FS_ORGANIZER_VIEWMODEL_REGISTERED_LIBRARY_H

#include <cstddef>

#include "domain/model/Library.h"

enum class LibraryCheck : int
{
    Accepted = 0,
    RejectedInsideAnotherLibrary = 1,
};

struct RegisteredLibrary
{
    Library library;
    std::size_t categories = 0;
    std::size_t addons = 0;
};

#endif // FS_ORGANIZER_VIEWMODEL_REGISTERED_LIBRARY_H
