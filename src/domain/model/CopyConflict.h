#ifndef FS_ORGANIZER_DOMAIN_MODEL_COPY_CONFLICT_H
#define FS_ORGANIZER_DOMAIN_MODEL_COPY_CONFLICT_H

#include <filesystem>

struct CopyConflict
{
    std::filesystem::path provenancePath{};
    std::filesystem::path libraryPath{};
    bool theProvenanceIsAnotherProgram = false;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_COPY_CONFLICT_H
