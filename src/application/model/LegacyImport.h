#ifndef FS_ORGANIZER_APPLICATION_MODEL_LEGACY_IMPORT_H
#define FS_ORGANIZER_APPLICATION_MODEL_LEGACY_IMPORT_H

#include <cstddef>
#include <filesystem>
#include <vector>

struct LegacyImportRequest
{
    std::vector<std::filesystem::path> libraryRoots;
    std::vector<std::filesystem::path> categories;
};

struct LegacyImportReport
{
    std::size_t librariesRegistered = 0;
    std::size_t categoriesDeclared = 0;
    std::vector<std::filesystem::path> refused;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_LEGACY_IMPORT_H
