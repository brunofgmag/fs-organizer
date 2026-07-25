#ifndef FS_ORGANIZER_INFRASTRUCTURE_ID_UUID_LIBRARY_ID_GENERATOR_H
#define FS_ORGANIZER_INFRASTRUCTURE_ID_UUID_LIBRARY_ID_GENERATOR_H

#include "application/ports/LibraryIdGenerator.h"

class UuidLibraryIdGenerator final : public LibraryIdGenerator
{
public:
    [[nodiscard]] LibraryId Generate() const override;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_ID_UUID_LIBRARY_ID_GENERATOR_H
