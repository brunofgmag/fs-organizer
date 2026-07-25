#ifndef FS_ORGANIZER_APPLICATION_PORTS_LIBRARY_ID_GENERATOR_H
#define FS_ORGANIZER_APPLICATION_PORTS_LIBRARY_ID_GENERATOR_H

#include "domain/model/LibraryId.h"

class LibraryIdGenerator
{
public:
    virtual ~LibraryIdGenerator() = default;

    [[nodiscard]] virtual LibraryId Generate() const = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_LIBRARY_ID_GENERATOR_H
